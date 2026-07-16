#pragma once

// ============================================================================
// SharpShooter AIO — Thresh
// Port từ CSharpFiles/Thresh/Thresh.cs (ImpulseAIO) sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h + SharpShooterAIO/Corki.h.
//
// Kỹ năng:
//   Q Death Sentence — hook 2 giai đoạn (ThreshQ = móc, ThreshQLeap = kéo mình
//     tới target). State machine đọc Q.Instance().Name().
//   W Dark Passage   — thả lồng đèn (shield/save ally); no-target, cast tới vị trí.
//   E Flay           — quét hất; PushE (đẩy ra) / PullE (kéo về, cast ngược hướng).
//   R The Box        — tường quanh 425; cast khi đủ số địch.
//
// Ghi chú port (giữ 1-1 tới mức API cho phép):
//   * Qedtarget: enemy có buff "ThreshQ" do MÌNH cast (so caster == player addr).
//   * Q2 mode: 0=Always, 1=Never, 2=Smart (điều kiện an toàn C#).
//   * Push/Pull key: IssueMove tới con trỏ + E theo hướng (đẩy/kéo).
//   * MISSING API: MissileManager.WillHit (chặn đạn cứu ally) → không có SDK
//     equiv, comment lại Helper(); xem missapi.md. Vẫn giữ W-save qua OnProcessSpell
//     (AA sát thủ vào ally → thả đèn).
//   * IssueOrder(MoveTo) → CoreControl::IssueMove.
//   * Thresh không có damage formula → không cần recheck wiki damage.
// ============================================================================

#include "../../../SDK/SDK.h"
#include "../../../core/CoreControl.h"
#include "../../../core/CoreBuffs.h"

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

namespace Plugins::SharpAIO::Thresh {

using SDK::Core::Utils::AutoAttack;

enum class CastState {
    NotReady,
    First,
    Second,
};

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* KeyMenu = nullptr;
inline Menu* MiscMenu = nullptr;
inline Menu* SafeMenu = nullptr;
inline Menu* DrawMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 1040.0f };
inline Spell W{ SpellSlot::W, 970.0f };
inline Spell PushE{ SpellSlot::E, 537.5f };
inline Spell PullE{ SpellSlot::E, 537.5f };
inline Spell R{ SpellSlot::R, 425.0f };

inline bool Loaded = false;

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

// C# Qedtarget: enemy dính buff "ThreshQ" do MÌNH cast.
static AIHeroClient Qedtarget() {
    const auto player = Player();
    if (!player.IsValid()) {
        return AIHeroClient();
    }
    for (const auto& e : GameObjects::EnemyHeroes()) {
        if (!e.IsValid() || !e.HasBuff("ThreshQ")) {
            continue;
        }
        const uintptr_t caster = e.GetBuffCaster("ThreshQ");
        if (caster != 0 && caster == player.Address()) {
            return e;
        }
    }
    return AIHeroClient();
}

// C# GetBuffLaveTime: thời gian còn lại của buff (endTime - now).
static float GetBuffLaveTime(const AIBaseClient& target, const char* buffName) {
    if (!target.IsValid()) {
        return 0.0f;
    }
    const auto buff = ::CoreBuffs::FindByName(target.Address(), buffName);
    if (!buff.IsValid()) {
        return 0.0f;
    }
    return buff.GetEndTime() - Game::Time();
}

// C# GetQState: đọc tên spell Q hiện tại (ThreshQ = giai đoạn 1, ThreshQLeap = 2).
static CastState GetQState() {
    if (!Q.IsReady()) {
        return CastState::NotReady;
    }
    const std::string name = Q.Instance().Name();
    if (name == "ThreshQ") {
        return CastState::First;
    }
    if (name == "ThreshQLeap") {
        return CastState::Second;
    }
    return CastState::NotReady;
}

// Forward declarations — đúng thứ tự file C#.
static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnProcessSpellCast(const Events::ProcessSpellEventArgs& args);
static void Helper();
static void Push_Pull();
static void AntiGapCloser(const GapCloserEventArgs& args);
static void WFarKeyLogic();
static void Combo();
static void Push(const AIBaseClient& target);
static void Pull(const AIBaseClient& target);
static void OnDraw();
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Thresh", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("CQ", "Use Q1"));
    ComboMenu->Add(new MenuList("CQ2Mode", "Use Q2 Mode",
        std::vector<std::string>{ "Always", "Never", "Smart" }, 2));
    ComboMenu->Add(new MenuBool("CW", "Use W"));
    ComboMenu->Add(new MenuKeyBind("WKey", "Cast W To MaxDistToPlayer Ally", 'T', KeyBindType::Press));
    ComboMenu->Add(new MenuBool("CE", "Use E"));
    ComboMenu->Add(new MenuBool("CR", "Use R"));
    ComboMenu->Add(new MenuSlider("CRS", "Use R if hitCount >= X", 2, 1, 5));

    KeyMenu = MenuRoot->AddSubMenu(new Menu("Push Pull Settings", "Push && Pull"));
    KeyMenu->Add(new MenuKeyBind("Pull", "Pull", 'A', KeyBindType::Press));
    KeyMenu->Add(new MenuKeyBind("Push", "Push", 'Z', KeyBindType::Press));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("AntiGap Settings", "AntiGap"));
    MiscMenu->Add(new MenuBool("EAntiGap", "Use E"));

    SafeMenu = MenuRoot->AddSubMenu(new Menu("Safe Settings", "E && W"));
    SafeMenu->Add(new MenuBool("EInterrupter", "Use E Interrupt"));
    SafeMenu->Add(new MenuBool("DLH", "Use W Protect Ally"));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Draw Settings", "Draw"));
    DrawMenu->Add(new MenuBool("DQ", "Draw Q"));
    DrawMenu->Add(new MenuBool("DE", "Draw E"));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 1040.0f);
    Q.SetSkillshot(0.5f, 70.0f, 1900.0f, true, SpellType::Line);
    Q.DamageType = DamageType::Magical;

    W = Spell(SpellSlot::W, 970.0f);

    PushE = Spell(SpellSlot::E, 537.5f);
    PushE.SetSkillshot(0.0f, 110.0f, 2000.0f, false, SpellType::Line);
    PushE.DamageType = DamageType::Magical;

    PullE = Spell(SpellSlot::E, 537.5f);
    PullE.SetSkillshot(0.0f, 110.0f, FLT_MAX, false, SpellType::Line);
    PullE.DamageType = DamageType::Magical;

    R = Spell(SpellSlot::R, 425.0f);

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnProcessSpell += &OnProcessSpellCast;
    Events::hook.OnGapCloser += &AntiGapCloser;
    Drawing::OnDraw += &OnDraw;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Thresh loaded</font>");
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (Game::IsChatOpen()) {
        return;
    }

    // Đang giai đoạn 2 của Q (leap) → khoá AA.
    Orbwalker::AttackEnabled(GetQState() != CastState::Second);

    Helper();
    Push_Pull();
    WFarKeyLogic();

    switch (Orbwalker::ActiveMode()) {
    case OrbwalkingMode::Combo:
        Combo();
        break;
    default:
        break;
    }
}

// C#: nếu địch AA sắp giết mình/ally → thả đèn W (shield/save).
static void OnProcessSpellCast(const Events::ProcessSpellEventArgs& args) {
    if (!Bool(SafeMenu, "DLH") || !W.IsReady()) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || !R.IsReady()) {
        return;
    }
    if (Events::IsLocalPlayer(args.Sender)) {
        return;
    }
    const auto sender = AIHeroClient(args.Sender.Ptr);
    if (!sender.IsValid() || !sender.IsEnemy()) {
        return;
    }
    if (!args.IsAutoAttack) {
        return;
    }

    const auto attackTarget = AIHeroClient(args.Target.Ptr);
    if (!attackTarget.IsValid()) {
        return;
    }
    if (attackTarget.IsMe()) {
        if (sender.GetAutoAttackDamage(AIBaseClient(player.Handle()), true) * 1.2f > player.Health()) {
            W.Cast(attackTarget.Position());
        }
    } else if (attackTarget.IsAlly() && attackTarget.DistanceToPlayer() <= W.Range) {
        if (sender.GetAutoAttackDamage(AIBaseClient(attackTarget.Handle()), true) * 1.2f > attackTarget.Health()) {
            W.Cast(attackTarget.Position());
        }
    }
}

// MISSING API: MissileManager.WillHit — không có SDK equiv (xem missapi.md).
// C# Helper() thả đèn W lên ally sắp trúng đạn. Bỏ vì không dò được missile→ally.
static void Helper() {
    if (!Bool(SafeMenu, "DLH")) {
        return;
    }
    // for (ally in range W) if (MissileManager.WillHit(ally)) W.Cast(ally.Position());
    // → BLOCKED: xem missapi.md (Thresh / MissileManager.WillHit).
}

static void Push_Pull() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (KeyActive(KeyMenu, "Pull")) {
        CoreControl::IssueMove(Game::CursorPos(), true);
        const auto t = GetTarget(PullE.Range, DamageType::Magical);
        if (ValidHeroTarget(t)) {
            Pull(AIBaseClient(t.Handle()));
        }
    }
    if (KeyActive(KeyMenu, "Push")) {
        CoreControl::IssueMove(Game::CursorPos(), true);
        const auto t = GetTarget(PushE.Range, DamageType::Magical);
        if (ValidHeroTarget(t)) {
            Push(AIBaseClient(t.Handle()));
        }
    }
}

static void AntiGapCloser(const GapCloserEventArgs& args) {
    if (!Bool(MiscMenu, "EAntiGap") || !PushE.IsReady()) {
        return;
    }
    const auto sender = AIHeroClient(args.Sender);
    if (ValidHeroTarget(sender, PushE.Range)) {
        Pull(AIBaseClient(sender.Handle()));
    }
}

static void WFarKeyLogic() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (KeyActive(ComboMenu, "WKey") && W.IsReady()) {
        if (player.CountAllyHeroesInRange(W.Range) - 1 > 0) {
            AIHeroClient farObj;
            int bestCount = -1;
            for (const auto& x : GameObjects::AllyHeroes()) {
                if (x.IsMe() || !ValidHeroTarget(x, W.Range)) {
                    continue;
                }
                const int c = x.CountEnemyHeroesInRange(450.0f);
                if (c > bestCount) {
                    bestCount = c;
                    farObj = x;
                }
            }
            if (farObj.IsValid()) {
                W.Cast(farObj.PreviousPosition().Extend(player.PreviousPosition(), 200.0f));
            }
        }
    }
}

static void Combo() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    const auto qed = Qedtarget();

    if (Bool(ComboMenu, "CE")) {
        const auto nornalt = GetTarget(PushE.Range, DamageType::Physical);
        if (ValidHeroTarget(nornalt) &&
            !(qed.IsValid() && nornalt.NetworkId() == qed.NetworkId())) {
            Pull(AIBaseClient(nornalt.Handle()));
        }
    }

    if (Bool(ComboMenu, "CQ")) {
        if (GetQState() == CastState::First) {
            const auto target = GetTarget(Q.Range, DamageType::Physical);
            if (ValidHeroTarget(target, Q.Range)) {
                const auto pred = Q.GetPrediction(target);
                if (HitchanceAtLeast(pred.Hitchance, HitChance::High) && pred.CollisionObjects.empty()) {
                    Q.Cast(pred.GetCastPosition());
                }
            }
        }
        if (GetQState() == CastState::Second) {
            const int q2Mode = ListIndex(ComboMenu, "CQ2Mode", 2);
            if (q2Mode != 1) {
                if (q2Mode == 0) {
                    if (qed.IsValid() && GetBuffLaveTime(AIBaseClient(qed.Handle()), "ThreshQ") < 0.4f) {
                        Q.Cast();
                    }
                }
                if (q2Mode == 2) {
                    if (qed.IsValid() && GetBuffLaveTime(AIBaseClient(qed.Handle()), "ThreshQ") < 0.4f &&
                        ((!qed.IsUnderEnemyTurret() &&
                          qed.CountEnemyHeroesInRange(600.0f) <= player.CountAllyHeroesInRange(1000.0f)) ||
                         (player.CountAllyHeroesInRange(800.0f) > 1 &&
                          qed.Health() <= Damage::GetAutoAttackDamage(player, AIBaseClient(qed.Handle())) * 10.0f))) {
                        Q.Cast();
                    }
                }
            }
        }
    }

    if (Bool(ComboMenu, "CR") && R.IsReady()) {
        if (player.CountEnemyHeroesInRange(R.Range) >= Slider(ComboMenu, "CRS", 2)) {
            R.Cast();
        }
    }

    if (Bool(ComboMenu, "CW") && W.IsReady()) {
        if (qed.IsValid() && player.IsDashing()) {
            if (qed.DistanceToPlayer() <= 600.0f) {
                AIHeroClient farAlly;
                float bestDist = -1.0f;
                for (const auto& x : GameObjects::AllyHeroes()) {
                    if (x.IsMe() || !ValidHeroTarget(x, W.Range)) {
                        continue;
                    }
                    const float d = x.DistanceToPlayer();
                    if (d <= W.Range && d > W.Range / 3.0f && d > bestDist) {
                        bestDist = d;
                        farAlly = x;
                    }
                }
                if (farAlly.IsValid()) {
                    W.Cast(farAlly.PreviousPosition());
                }
            } else {
                AIHeroClient farAlly;
                float bestDist = FLT_MAX;
                for (const auto& x : GameObjects::AllyHeroes()) {
                    if (x.IsMe() || x.IsDead() || !ValidHeroTarget(x, W.Range)) {
                        continue;
                    }
                    const float d = x.DistanceToPlayer();
                    if (d <= W.Range &&
                        !(AutoAttack::GetRealAutoAttackRange(x) > qed.Distance(x)) &&
                        d < bestDist) {
                        bestDist = d;
                        farAlly = x;
                    }
                }
                if (farAlly.IsValid()) {
                    W.Cast(farAlly.PreviousPosition());
                }
            }
        }
    }
}

static void Push(const AIBaseClient& target) {
    if (PushE.IsReady() && ValidTarget(target, PushE.Range) && target.IsEnemy()) {
        const auto pred = PushE.GetPrediction(target);
        if (HitchanceAtLeast(pred.Hitchance, HitChance::Medium)) {
            const auto player = Player();
            if (player.IsValid()) {
                PushE.Cast(player.Position().Extend(pred.GetCastPosition(), PushE.Range - 5.0f));
            }
        }
    }
}

static void Pull(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (PullE.IsReady() && ValidTarget(target, PullE.Range) && target.IsEnemy()) {
        const auto pred = PullE.GetPrediction(target);
        if (HitchanceAtLeast(pred.Hitchance, HitChance::Medium)) {
            const Vector3 ext = player.Position().Extend(
                pred.GetCastPosition(), -(PullE.Range - 5.0f));
            PullE.Cast(ext);
        }
    }
}

static void OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (Bool(DrawMenu, "DQ", false) && Q.IsReady()) {
        Drawing::DrawCircle(player.Position(), Q.Range, 0xFFFF0000u);
    }
    if (Bool(DrawMenu, "DE", false) && PullE.IsReady()) {
        Drawing::DrawCircle(player.Position(), PullE.Range, 0xFFFFA500u);
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnProcessSpell -= &OnProcessSpellCast;
    Events::hook.OnGapCloser -= &AntiGapCloser;
    Drawing::OnDraw -= &OnDraw;

    Loaded = false;
}

} // namespace Plugins::SharpAIO::Thresh
