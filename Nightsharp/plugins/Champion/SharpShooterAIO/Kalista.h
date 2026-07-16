#pragma once

// ============================================================================
// SharpShooter AIO — Kalista
// Port từ SharpShooterCSHarp/Plugins/Kalista.cs sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h.
//
// Kỹ năng:
//   Q Pierce   — skillshot line 1150, delay 0.35, width 40, speed 2400,
//                collision (minion) true. Cast no-collision trong combo/harass;
//                laneclear bắn khi giết được >= N lính xuyên hàng; jungle mob.
//   W Sentinel — global scout (5000). Auto cast lên Baron/Dragon khi rảnh + keybind.
//   E Rend     — không target (Cast()). Killsteal / mobsteal / lasthit assist /
//                siege+super steal / auto-E-before-die. Gate 700ms chống spam.
//   R Fate's Call — Soulbound saver (cứu đồng minh <20% HP bị nhắm) + balista combo.
//
// Ghi chú port:
//   * E cast gate: bản C# chặn re-cast E trong 700ms qua Spellbook.OnCastSpell.
//     Ở đây dùng ELastCastTick + helper CastE() (không có inbound cast hook).
//   * HealthPrediction.GetHealthPrediction(x, ms) → Prediction::Health::GetPrediction.
//   * Baron/Dragon location hardcode giống bản C#.
// ============================================================================

#include "../../../SDK/SDK.h"
#include "../../../core/CoreControl.h"

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

namespace Plugins::SharpAIO::Kalista {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* MiscMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 1150.0f };
inline Spell W{ SpellSlot::W, 5000.0f };
inline Spell E{ SpellSlot::E, 1000.0f };
inline Spell R{ SpellSlot::R, 1500.0f };

inline bool Loaded = false;
inline DWORD ELastCastTick = 0;

inline const Vector3 BaronLocation{ 5064.0f, 10568.0f, -71.0f };
inline const Vector3 DragonLocation{ 9796.0f, 4432.0f, -71.0f };

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

static bool KeyActive(Menu* menu, const char* key) {
    if (!menu) {
        return false;
    }
    const auto* item = menu->Get<MenuKeyBind>(key);
    return item && item->Active;
}

static bool ManaOkay(int percent) {
    const auto player = Player();
    return player.IsValid() && player.ManaPercent() >= static_cast<float>(percent);
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

static AIHeroClient GetTargetNoCollision(Spell& spell) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTargetNoCollision(&spell) : AIHeroClient();
}

// C# GetBestTarget: nếu bật attackW, ưu tiên target đã dính buff oath-partner mark
// ("kalistacoopstrikemarkally") để kích Sentinel passive; không có thì target
// thường. Trả về hero tốt nhất trong tầm.
static AIHeroClient GetBestTarget(float range) {
    auto* selector = SDK::TargetSelector::Instance();
    if (!selector) {
        return AIHeroClient();
    }
    if (Bool(ComboMenu, "attackW", true)) {
        const auto targets = selector->GetTargets(range, DamageType::Physical);
        for (const auto& t : targets) {
            if (t.HasBuff("kalistacoopstrikemarkally")) {
                return t;
            }
        }
    }
    return selector->GetTarget(range, DamageType::Physical);
}

// Killable rút gọn (loại buff bất tử + so máu/shield vật lý).
static bool IsKillable(const AIBaseClient& target, double damage) {
    if (!ValidUnit(target)) {
        return false;
    }
    if (target.HasBuff("kindredrnodeathbuff") || target.HasBuff("Undying Rage") ||
        target.HasBuff("JudicatorIntervention") || target.HasBuff("BansheesVeil") ||
        target.HasBuff("SivirShield") || target.HasBuff("ShroudofDarkness")) {
        return false;
    }
    return target.Health() + target.PhysicalShield() < damage - 2.0;
}

static float HealthPred(const AIBaseClient& unit, int ms) {
    return unit.IsValid() ? Prediction::Health::GetPrediction(unit, ms) : 0.0f;
}

// Số spear (giáo) đang cắm trên target — quyết định damage của E Rend.
// Buff "kalistaexpungemarker" stack theo mỗi lần AA/Q trúng (mỗi stack = 1 giáo).
// GetBuffCount có sẵn ở AIBaseClient (SDK/Core/Objects.h). 0 giáo → E vô hại.
static int SpearCount(const AIBaseClient& target) {
    if (!target.IsValid()) {
        return 0;
    }
    return target.GetBuffCount("kalistaexpungemarker");
}

// ── Damage tính tay theo wiki (leagueoflegends.com/en-us/Kalista/LoL) ──
// KHÔNG dùng Spell::GetDamage — số liệu chính xác theo bảng wiki (patch hiện tại).
//
// Q Pierce (PHYSICAL, skillshot line):
//     base 10 / 75 / 140 / 205 / 270  (+ 105% AD tổng)
//
// W Sentinel: KHÔNG có nuke trực tiếp (chỉ scout/patrol + đánh dấu Soul-Marked).
//
// E Rend (PHYSICAL, không target — expunge toàn bộ giáo đang cắm):
//     Giáo đầu tiên : 5 / 15 / 25 / 35 / 45          (+ 70% AD tổng) (+ 65% AP)
//     Mỗi giáo thêm : 7 / 14 / 21 / 28 / 35 base
//                     (+ 20 / 27.5 / 35 / 42.5 / 50% AD tổng) (+ 50% AP)
//     Tổng = firstSpear + (spearCount - 1) * perSpear ;  spearCount<1 → 0.
//     (spearCount lấy từ SpearCount(target); nếu không có giáo → E không sát thương.)
//
// R Fate's Call: KHÔNG sát thương (cleanse + dash đồng minh gắn kết, knock-up là CC).
//
// Trả về damage đã trừ giáp qua Damage::CalculateDamage (DamageType::Physical).
static float SpellDamage(SpellSlot slot, const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }
    const float totalAd = player.TotalAttackDamage();
    const float ap = player.AP();

    switch (slot) {
    case SpellSlot::Q: {
        const int rank = Q.Instance().Level();
        if (rank < 1) {
            return 0.0f;
        }
        static const float base[5] = { 10.0f, 75.0f, 140.0f, 205.0f, 270.0f };
        const float raw = base[rank - 1] + 1.05f * totalAd; // 105% AD tổng
        return Damage::CalculateDamage(player, target, DamageType::Physical, raw);
    }
    case SpellSlot::E: {
        const int rank = E.Instance().Level();
        if (rank < 1) {
            return 0.0f;
        }
        const int spears = SpearCount(target);
        if (spears < 1) {
            return 0.0f; // không giáo → Rend vô hại.
        }
        static const float firstBase[5] = { 5.0f, 15.0f, 25.0f, 35.0f, 45.0f };
        static const float perBase[5] = { 7.0f, 14.0f, 21.0f, 28.0f, 35.0f };
        static const float perAdRatio[5] = { 0.20f, 0.275f, 0.35f, 0.425f, 0.50f };
        const int idx = rank - 1;
        // Giáo đầu: base + 70% AD + 65% AP.
        float raw = firstBase[idx] + 0.70f * totalAd + 0.65f * ap;
        // Mỗi giáo bổ sung.
        const int extra = spears - 1;
        if (extra > 0) {
            raw += static_cast<float>(extra) *
                (perBase[idx] + perAdRatio[idx] * totalAd + 0.50f * ap);
        }
        if (target.IsMinion()) {
            const AIMinionClient minion(target.Handle());
            if (minion.IsValid() &&
                (minion.GetJungleType() & JungleType::Legendary) != JungleType::Unknown) {
                raw *= 0.5f;
            }
        }
        return Damage::CalculateDamage(player, target, DamageType::Physical, raw);
    }
    default:
        return 0.0f; // W/R không có sát thương trực tiếp.
    }
}

// E Rend: cast một lần, gate 700ms chống spam (thay Spellbook.OnCastSpell).
static void CastE() {
    if (!E.IsReady() || ELastCastTick + 700 > GetTickCount()) {
        return;
    }
    if (E.Cast()) {
        ELastCastTick = GetTickCount();
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnNonKillableMinion(OrbwalkingActionArgs& args);
static void OnProcessSpell(const ProcessSpellEventArgs& args);
static void Combo();
static void Mixed();
static void Clear();
static void Misc();
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Kalista", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("useQ", "Use Q"));
    ComboMenu->Add(new MenuBool("useE", "Use E (rend finish)"));
    ComboMenu->Add(new MenuBool("attackW", "Prefer Soul-marked target (W passive)", true));
    ComboMenu->Add(new MenuBool("gapAttack", "Attack minion to gap-close", false));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("useQ", "Use Q"));
    HarassMenu->Add(new MenuSlider("Mana", "If Mana > %", 60, 0, 100));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear Settings", "Lane Clear"));
    LaneClearMenu->Add(new MenuBool("useQ", "Use Q", false));
    LaneClearMenu->Add(new MenuSlider("qKillCount", "Cast Q if killable minions >=", 3, 1, 7));
    LaneClearMenu->Add(new MenuBool("useE", "Use E"));
    LaneClearMenu->Add(new MenuSlider("eKillCount", "Cast E if killable minions >=", 2, 1, 5));
    LaneClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 20, 0, 100));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("Jungle Settings", "Jungle Clear"));
    JungleClearMenu->Add(new MenuBool("useQ", "Use Q"));
    JungleClearMenu->Add(new MenuBool("useE", "Use E"));
    JungleClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 20, 0, 100));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    MiscMenu->Add(new MenuBool("killsteal", "Killsteal (E/Q)"));
    MiscMenu->Add(new MenuBool("mobsteal", "Mobsteal (E)"));
    MiscMenu->Add(new MenuBool("lasthitAssist", "Lasthit Assist (E)"));
    MiscMenu->Add(new MenuBool("siegeSteal", "Steal Siege/Super minion (E)"));
    MiscMenu->Add(new MenuBool("autoEBeforeDie", "Auto E before die"));
    MiscMenu->Add(new MenuBool("soulboundSaver", "Soulbound Saver (R)"));
    MiscMenu->Add(new MenuBool("balista", "Auto Balista Combo (R)"));
    MiscMenu->Add(new MenuBool("autoWEpic", "Auto W on Dragon/Baron"));
    MiscMenu->Add(new MenuKeyBind("wDragon", "Cast W on Dragon", 'J', KeyBindType::Press));
    MiscMenu->Add(new MenuKeyBind("wBaron", "Cast W on Baron", 'K', KeyBindType::Press));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 1150.0f);
    Q.SetSkillshot(0.35f, 40.0f, 2400.0f, true, SpellType::Line);
    Q.DamageType = DamageType::Physical;

    W = Spell(SpellSlot::W, 5000.0f);
    E = Spell(SpellSlot::E, 1000.0f);
    E.DamageType = DamageType::Physical;
    R = Spell(SpellSlot::R, 1500.0f);

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnProcessSpell += &OnProcessSpell;
    Orbwalker::OnNonKillableMinion += &OnNonKillableMinion;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Kalista loaded</font>");
}

// Soulbound saver + Auto-E-before-die: phản ứng khi địch cast spell.
static void OnProcessSpell(const ProcessSpellEventArgs& args) {
    if (!Loaded || args.Target.Ptr == 0) {
        return;
    }
    const AIHeroClient sender(([&]() {
        ::Core::Objects::ObjectHandle h{};
        h.address = args.Sender.Ptr;
        h.index = args.Sender.Index;
        h.networkId = args.Sender.NetworkId;
        h.type = args.Sender.Type;
        return h;
    })());
    if (!sender.IsValid() || !sender.IsHero() || !sender.IsEnemy()) {
        return;
    }

    // Soulbound Saver: cứu đồng minh gắn kết <20% HP bị nhắm.
    if (Bool(MiscMenu, "soulboundSaver") && R.IsReady()) {
        for (const auto& ally : GameObjects::AllyHeroes()) {
            if (ally.IsDead() || !ally.HasBuff("kalistacoopstrikeally")) {
                continue;
            }
            const bool targeted = args.Target.NetworkId == ally.NetworkId() ||
                args.EndPosition.Distance2D(ally.Position()) <= 200.0f;
            if (targeted && ally.HealthPercent() < 20.0f) {
                R.Cast();
                break;
            }
        }
    }

    // Auto E before die: mình bị nhắm và HP <=10%.
    if (Bool(MiscMenu, "autoEBeforeDie") && E.IsReady()) {
        const auto player = Player();
        if (player.IsValid() && player.NetworkId() == static_cast<int>(args.Target.NetworkId) &&
            player.HealthPercent() <= 10.0f) {
            for (const auto& enemy : GameObjects::EnemyHeroes()) {
                if (ValidHeroTarget(enemy, E.Range) && SpellDamage(SpellSlot::E, enemy) > 0.0f) {
                    CastE();
                    break;
                }
            }
        }
    }
}

// Lasthit assist: khi orbwalker báo minion không giết được bằng AA, dùng E.
static void OnNonKillableMinion(OrbwalkingActionArgs& args) {
    if (!Loaded || !Bool(MiscMenu, "lasthitAssist") || !E.IsReady()) {
        return;
    }
    const auto minion = AIMinionClient(args.Target.Handle());
    if (!ValidUnit(minion) || HealthPred(AIBaseClient(minion.Handle()), 250) <= 0.0f) {
        return;
    }
    if (!IsKillable(AIBaseClient(minion.Handle()), SpellDamage(SpellSlot::E, AIBaseClient(minion.Handle())))) {
        return;
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(enemy) && AutoAttack::InAutoAttackRange(enemy)) {
            return; // ưu tiên đánh hero, không phí E vào lính.
        }
    }
    CastE();
}

static void Combo() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (Bool(ComboMenu, "useQ") && Q.IsReady() && !player.IsDashing() &&
        !player.Spellbook().IsAutoAttack()) {
        // Ưu tiên target Soul-marked (attackW) nếu bật; fallback no-collision TS.
        auto target = GetBestTarget(Q.Range);
        if (!ValidHeroTarget(target, Q.Range)) {
            target = GetTargetNoCollision(Q);
        }
        if (ValidHeroTarget(target, Q.Range)) {
            Q.Cast(target);
        }
    }

    if (Bool(ComboMenu, "useE") && E.IsReady()) {
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            // -30 giữ margin an toàn (bù pred/latency) như bản gốc.
            if (ValidHeroTarget(enemy, E.Range) && HealthPred(AIBaseClient(enemy.Handle()), 250) > 0.0f &&
                IsKillable(enemy, SpellDamage(SpellSlot::E, enemy) - 30.0)) {
                CastE();
                break;
            }
        }
    }

    // C# ComboGap: khi orbwalker không có target hero nhưng có địch quanh (2000),
    // đánh con lính máu thấp nhất trong tầm AA để tiến gần (Kalista dịch chuyển
    // theo mỗi đòn đánh → dùng AA lính làm gap-close).
    if (Bool(ComboMenu, "gapAttack", false) && Orbwalker::CanAttack()) {
        const auto orbTarget = Orbwalker::GetTarget();
        if (!orbTarget.IsValid() && player.CountEnemyHeroesInRange(2000.0f) > 0) {
            const float aaRange = AutoAttack::GetRealAutoAttackRange(player, AttackableUnit());
            AIMinionClient best;
            float bestHp = FLT_MAX;
            for (const auto& minion : GameObjects::EnemyMinions()) {
                if (ValidTarget(minion, aaRange) && minion.Health() < bestHp) {
                    bestHp = minion.Health();
                    best = minion;
                }
            }
            if (best.IsValid()) {
                CoreControl::IssueAttack(best.Address(), best.Position());
            }
        }
    }
}

static void Mixed() {
    const auto player = Player();
    if (!player.IsValid() || !Bool(HarassMenu, "useQ") || !Q.IsReady()) {
        return;
    }
    if (!ManaOkay(Slider(HarassMenu, "Mana", 60)) || player.IsDashing() ||
        player.Spellbook().IsAutoAttack()) {
        return;
    }
    const auto target = GetTargetNoCollision(Q);
    if (ValidHeroTarget(target, Q.Range)) {
        Q.Cast(target);
    }
}

static void Clear() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    // Lane clear Q: bắn khi hàng lính xuyên qua giết được >= N.
    if (Bool(LaneClearMenu, "useQ", false) && Q.IsReady() && !player.IsDashing() &&
        !player.Spellbook().IsAutoAttack() && ManaOkay(Slider(LaneClearMenu, "Mana", 20))) {
        const int need = Slider(LaneClearMenu, "qKillCount", 3);
        auto minions = GameObjects::EnemyLaneMinions();
        if (minions.empty()) {
            minions = GameObjects::EnemyMinions();
        }
        int killable = 0;
        Vector3 castPos{};
        for (const auto& minion : minions) {
            if (ValidTarget(minion, Q.Range) &&
                IsKillable(AIBaseClient(minion.Handle()), SpellDamage(SpellSlot::Q, AIBaseClient(minion.Handle())))) {
                ++killable;
                if (!castPos.IsValid() || castPos.IsZero()) {
                    castPos = minion.ServerPosition();
                }
            }
        }
        if (killable >= need && castPos.IsValid() && !castPos.IsZero()) {
            Q.Cast(castPos);
        }
    }

    // Lane clear E: cast khi >= N lính giết được bằng E.
    if (Bool(LaneClearMenu, "useE") && E.IsReady() && ManaOkay(Slider(LaneClearMenu, "Mana", 20))) {
        const int need = Slider(LaneClearMenu, "eKillCount", 2);
        int killable = 0;
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (ValidTarget(minion, E.Range) && HealthPred(AIBaseClient(minion.Handle()), 250) > 0.0f &&
                IsKillable(AIBaseClient(minion.Handle()), SpellDamage(SpellSlot::E, AIBaseClient(minion.Handle())))) {
                ++killable;
            }
        }
        if (killable >= need) {
            CastE();
        }
    }

    // Jungle clear Q + E: mob máu cao nhất trong tầm.
    auto mobs = GameObjects::Jungle();
    mobs.erase(
        std::remove_if(
            mobs.begin(),
            mobs.end(),
            [](const AIMinionClient& mob) {
                return !ValidTarget(mob, E.Range) || mob.IsPlant() || mob.IsPet();
            }),
        mobs.end());
    std::sort(
        mobs.begin(),
        mobs.end(),
        [](const AIMinionClient& a, const AIMinionClient& b) {
            return a.MaxHealth() > b.MaxHealth();
        });

    if (!mobs.empty()) {
        const auto& mob = mobs.front();
        if (Bool(JungleClearMenu, "useQ") && Q.IsReady() && ManaOkay(Slider(JungleClearMenu, "Mana", 20)) &&
            ValidTarget(mob, Q.Range)) {
            const auto pred = Q.GetPrediction(mob);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                Q.Cast(mob);
            }
        }
        if (Bool(JungleClearMenu, "useE") && E.IsReady() && ManaOkay(Slider(JungleClearMenu, "Mana", 20)) &&
            HealthPred(AIBaseClient(mob.Handle()), 250) > 0.0f &&
            IsKillable(AIBaseClient(mob.Handle()), SpellDamage(SpellSlot::E, AIBaseClient(mob.Handle())))) {
            CastE();
        }
    }
}

static void Misc() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    // Killsteal: E Rend (expunge giáo) + Q Pierce. Chạy mọi mode, gate bởi "killsteal".
    if (Bool(MiscMenu, "killsteal")) {
        // E Rend trước: instant, không cần pred vị trí (chỉ cần có giáo cắm sẵn).
        if (E.IsReady()) {
            for (const auto& enemy : GameObjects::EnemyHeroes()) {
                // -30 giữ margin an toàn (bù pred/latency) như bản gốc.
                if (ValidHeroTarget(enemy, E.Range) && HealthPred(AIBaseClient(enemy.Handle()), 250) > 0.0f &&
                    IsKillable(enemy, SpellDamage(SpellSlot::E, enemy) - 30.0)) {
                    CastE();
                    return;
                }
            }
        }
        // Q Pierce: skillshot line, chỉ bắn khi hitchance đủ cao.
        if (Q.IsReady() && !player.IsDashing() && !player.Spellbook().IsAutoAttack()) {
            const auto target = GetTargetNoCollision(Q);
            if (ValidHeroTarget(target, Q.Range) && IsKillable(target, SpellDamage(SpellSlot::Q, target))) {
                const auto pred = Q.GetPrediction(target);
                if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                    Q.Cast(pred.GetCastPosition());
                    return;
                }
            }
        }
    }

    // Mobsteal + siege/super steal (E) trên quái/lính.
    if ((Bool(MiscMenu, "mobsteal") || Bool(MiscMenu, "siegeSteal")) && E.IsReady()) {
        if (Bool(MiscMenu, "mobsteal")) {
            for (const auto& mob : GameObjects::Jungle()) {
                if (ValidTarget(mob, E.Range) && HealthPred(AIBaseClient(mob.Handle()), 500) > 0.0f &&
                    IsKillable(AIBaseClient(mob.Handle()), SpellDamage(SpellSlot::E, AIBaseClient(mob.Handle())))) {
                    CastE();
                    return;
                }
            }
        }
        if (Bool(MiscMenu, "siegeSteal")) {
            for (const auto& minion : GameObjects::EnemyMinions()) {
                if (!ValidTarget(minion, E.Range) || HealthPred(AIBaseClient(minion.Handle()), 250) <= 0.0f) {
                    continue;
                }
                if (!IsKillable(AIBaseClient(minion.Handle()), SpellDamage(SpellSlot::E, AIBaseClient(minion.Handle())))) {
                    continue;
                }
                const std::string name = minion.CharacterName();
                std::string lower = name;
                std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });
                if (lower.find("siege") != std::string::npos || lower.find("super") != std::string::npos) {
                    CastE();
                    return;
                }
            }
        }
    }

    // Balista combo (R): có Blitzcrank đồng minh gắn kết + địch bị Rocket Grab.
    if (Bool(MiscMenu, "balista") && R.IsReady()) {
        bool haveBlitz = false;
        for (const auto& ally : GameObjects::AllyHeroes()) {
            if (!ally.IsDead() && ally.HasBuff("kalistacoopstrikeally") &&
                _stricmp(ally.CharacterName().c_str(), "Blitzcrank") == 0) {
                haveBlitz = true;
                break;
            }
        }
        if (haveBlitz) {
            for (const auto& enemy : GameObjects::EnemyHeroes()) {
                if (!enemy.IsDead() && enemy.HasBuff("rocketgrab2")) {
                    R.Cast();
                    break;
                }
            }
        }
    }

    // Auto W lên Baron/Dragon khi rảnh (không địch quanh, không có target orbwalk).
    if (Bool(MiscMenu, "autoWEpic") && W.IsReady() && ManaOkay(50) && !player.IsRecalling() &&
        player.CountEnemyHeroesInRange(1500.0f) == 0) {
        if (player.Position().Distance(BaronLocation) <= W.Range) {
            W.Cast(BaronLocation);
        } else if (player.Position().Distance(DragonLocation) <= W.Range) {
            W.Cast(DragonLocation);
        }
    }

    // Keybind W thủ công.
    if (W.IsReady()) {
        if (KeyActive(MiscMenu, "wDragon") && player.Position().Distance(DragonLocation) <= W.Range) {
            W.Cast(DragonLocation);
        }
        if (KeyActive(MiscMenu, "wBaron") && player.Position().Distance(BaronLocation) <= W.Range) {
            W.Cast(BaronLocation);
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

    switch (Orbwalker::ActiveMode()) {
    case OrbwalkingMode::Combo:
        Combo();
        break;
    case OrbwalkingMode::Harass:
        Mixed();
        break;
    case OrbwalkingMode::LaneClear:
        Clear();
        break;
    default:
        break;
    }

    Misc();
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnProcessSpell -= &OnProcessSpell;
    Orbwalker::OnNonKillableMinion -= &OnNonKillableMinion;

    Loaded = false;
}

} // namespace Plugins::SharpAIO::Kalista
