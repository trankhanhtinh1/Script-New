#pragma once

// ============================================================================
// SharpShooter AIO — Lucian
// Port từ SharpShooterCSHarp/Plugins/Lucian.cs sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h.
//
// Kỹ năng:
//   Q Piercing Light  — targeted 675 (nội bộ là line 900 xuyên qua unit đầu,
//                       _qExtended). Có thể cast lên minion để xuyên trúng hero
//                       phía sau (box rectangle check).
//   W Ardent Blaze    — skillshot line 1000, delay 0.30, width 55, speed 1600.
//                       Hai biến thể: có collision (_w) và không (_wNoCollision).
//   E Relentless Pursuit — dash 475, cast theo hướng (anti-melee / anti-gapcloser /
//                       reposition sau đòn đánh).
//   R The Culling      — không tự cast (chỉ chặn orbwalk khi đang bắn "LucianR").
//
// Cơ chế Lightslinger (passive): sau mỗi kỹ năng, đòn đánh kế tiếp bắn 2 phát.
//   _hasPassive = true khi vừa cast Q/W/E/R (OnProcessSpell), reset false sau khi
//   đánh thường (OnAfterAttack) hoặc mất buff "lucianpassivebuff" (OnBuffRemove).
//   Khi _hasPassive: ưu tiên finish mục tiêu killable thay vì poke.
//
// Ghi chú port:
//   * Q-xuyên-minion dùng RectanglePoly + IsInside, tương đương Geometry.Rectangle.
//   * PlayAnimation "Spell1"/"Spell2" → IssueMove tới cursor để huỷ hậu-swing
//     (giữ nhịp orbwalk), giống bản C#.
// ============================================================================

#include "../../../SDK/SDK.h"
#include "../../../core/CoreControl.h"

#include <algorithm>
#include <cfloat>
#include <cstring>
#include <string>
#include <vector>

namespace Plugins::SharpAIO::Lucian {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* MiscMenu = nullptr;

// _q: targeted 675. _qExtended: line 900 xuyên unit (prediction cho xuyên minion).
inline Spell Q{ SpellSlot::Q, 675.0f };
inline Spell QExtended{ SpellSlot::Q, 900.0f };
inline Spell W{ SpellSlot::W, 1000.0f };            // có collision (minion)
inline Spell WNoCollision{ SpellSlot::W, 1000.0f }; // không collision
inline Spell E{ SpellSlot::E, 475.0f };
inline Spell R{ SpellSlot::R, 1400.0f };

inline bool Loaded = false;
inline bool HasPassive = false;
inline DWORD LastComboEvalTick = 0;

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

static bool ShouldRunNow(DWORD& lastTick, DWORD intervalMs) {
    const DWORD now = GetTickCount();
    if (lastTick != 0 && now - lastTick < intervalMs) {
        return false;
    }
    lastTick = now;
    return true;
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

static AIHeroClient GetTarget(float range, DamageType damageType) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTarget(range, damageType) : AIHeroClient();
}

static AIHeroClient GetTargetNoCollision(Spell& spell) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTargetNoCollision(&spell) : AIHeroClient();
}

// Dựng AIHeroClient từ ObjectInfo (named-field, không phụ thuộc thứ tự struct).
static AIHeroClient HeroFromInfo(const Core::Events::ObjectInfo& info) {
    ::Core::Objects::ObjectHandle handle{};
    handle.address = info.Ptr;
    handle.index = info.Index;
    handle.networkId = info.NetworkId;
    handle.type = info.Type;
    return AIHeroClient(handle);
}

// Killable: so máu + shield vật lý với sát thương (rút gọn IsKillableAndValidTarget).
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

// Q xuyên minion để trúng hero phía sau: tìm minion mà khi bắn xuyên (line tới
// _qExtended.Range) sẽ trúng extendedTarget đã predict.
static bool CastExtendedQThroughMinion(const AIHeroClient& extendedTarget) {
    const auto player = Player();
    if (!player.IsValid() || !ValidHeroTarget(extendedTarget, QExtended.Range)) {
        return false;
    }

    const Vector3 playerServer = player.ServerPosition();
    const auto pred = QExtended.GetPrediction(extendedTarget);
    if (!HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
        return false;
    }
    const Vector3 predPos = pred.GetUnitPosition();

    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (!ValidTarget(minion, Q.Range)) {
            continue;
        }
        const Vector3 boxEnd = playerServer.Extend(minion.ServerPosition(), QExtended.Range);
        SDK::RectanglePoly box(playerServer, boxEnd, QExtended.Width);
        if (box.IsInside(predPos)) {
            return Q.CastOnUnit(AIBaseClient(minion.Handle()));
        }
    }
    return false;
}

static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnProcessSpell(const ProcessSpellEventArgs& args);
static void OnPlayAnimation(const PlayAnimationEventArgs& args);
static void OnBuffRemove(const BuffEventArgs& args);
static void OnBeforeAttack(OrbwalkingActionArgs& args);
static void OnAfterAttack(OrbwalkingActionArgs& args);
static void Gapcloser_OnGapcloser(const GapCloserEventArgs& args);
static void Combo();
static void Mixed();
static void Clear();
static void AutoHarass();
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Lucian", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("useQ", "Use Q"));
    ComboMenu->Add(new MenuBool("useW", "Use W"));
    ComboMenu->Add(new MenuBool("useE", "Use E (reposition)"));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("useQ", "Use Q"));
    HarassMenu->Add(new MenuBool("useW", "Use W"));
    HarassMenu->Add(new MenuBool("autoHarass", "Auto Harass"));
    HarassMenu->Add(new MenuSlider("Mana", "If Mana > %", 0, 0, 100));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear Settings", "Lane Clear"));
    LaneClearMenu->Add(new MenuBool("useQ", "Use Q", false));
    LaneClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 60, 0, 100));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("Jungle Settings", "Jungle Clear"));
    JungleClearMenu->Add(new MenuBool("useQ", "Use Q"));
    JungleClearMenu->Add(new MenuBool("useW", "Use W"));
    JungleClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 20, 0, 100));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    MiscMenu->Add(new MenuBool("gapcloser", "Anti-Gapcloser (E)"));
    MiscMenu->Add(new MenuBool("antiMelee", "Use Anti-Melee (E)"));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 675.0f);
    Q.DamageType = DamageType::Physical;

    QExtended = Spell(SpellSlot::Q, 900.0f);
    QExtended.SetSkillshot(0.5f, 65.0f, FLT_MAX, false, SpellType::Line);
    QExtended.DamageType = DamageType::Physical;

    W = Spell(SpellSlot::W, 1000.0f);
    W.SetSkillshot(0.30f, 55.0f, 1600.0f, true, SpellType::Line);
    W.DamageType = DamageType::Physical;

    WNoCollision = Spell(SpellSlot::W, 1000.0f);
    WNoCollision.SetSkillshot(0.30f, 55.0f, 1600.0f, false, SpellType::Line);
    WNoCollision.DamageType = DamageType::Physical;

    E = Spell(SpellSlot::E, 475.0f);
    R = Spell(SpellSlot::R, 1400.0f);

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnProcessSpell += &OnProcessSpell;
    Events::hook.OnPlayAnimation += &OnPlayAnimation;
    Events::hook.OnBuffRemove += &OnBuffRemove;
    Orbwalker::OnBeforeAttack += &OnBeforeAttack;
    Orbwalker::OnAfterAttack += &OnAfterAttack;
    Events::hook.OnGapCloser += &Gapcloser_OnGapcloser;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Lucian loaded</font>");
}

static void OnProcessSpell(const ProcessSpellEventArgs& args) {
    if (!Loaded) {
        return;
    }

    // Vừa cast Q/W/E/R → nạp passive Lightslinger.
    if (Events::IsLocalPlayer(args.Sender)) {
        if (args.Slot == static_cast<int>(SpellSlot::Q) ||
            args.Slot == static_cast<int>(SpellSlot::W) ||
            args.Slot == static_cast<int>(SpellSlot::E) ||
            args.Slot == static_cast<int>(SpellSlot::R)) {
            HasPassive = true;
        }
        return;
    }

    // Anti-melee E: địch cận chiến auto-attack mình → E lùi ra sau.
    if (!Bool(MiscMenu, "antiMelee") || !E.IsReady() || !args.IsAutoAttack) {
        return;
    }
    if (args.Target.Ptr == 0) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid() || player.NetworkId() != static_cast<int>(args.Target.NetworkId)) {
        return;
    }

    const AIHeroClient sender = HeroFromInfo(args.Sender);
    if (sender.IsValid() && sender.IsHero() && sender.IsEnemy() && sender.IsMelee()) {
        E.Cast(player.Position().Extend(sender.Position(), -E.Range));
    }
}

// Huỷ hậu-swing sau khi bắn kỹ năng (Spell1/Spell2) để giữ nhịp orbwalk.
static void OnPlayAnimation(const PlayAnimationEventArgs& args) {
    if (!Loaded || !Events::IsLocalPlayer(args.Sender)) {
        return;
    }
    if ((std::strcmp(args.Animation, "Spell1") == 0 || std::strcmp(args.Animation, "Spell2") == 0) &&
        Orbwalker::ActiveMode() != OrbwalkingMode::None) {
        CoreControl::IssueMove(Game::CursorPos(), true);
    }
}

static void OnBuffRemove(const BuffEventArgs& args) {
    if (!Loaded || !Events::IsLocalPlayer(args.Sender)) {
        return;
    }
    if (_stricmp(args.BuffName, "lucianpassivebuff") == 0) {
        HasPassive = false;
    }
}

// Chặn orbwalk khi đang bắn R (The Culling).
static void OnBeforeAttack(OrbwalkingActionArgs& args) {
    if (!Loaded) {
        return;
    }
    const auto player = Player();
    if (player.IsValid() && player.HasBuff("LucianR")) {
        args.Process = false;
    }
}

// Sau đòn đánh: reset passive; E reposition trong combo (nếu ít địch quanh cursor).
static void OnAfterAttack(OrbwalkingActionArgs&) {
    if (!Loaded) {
        return;
    }
    HasPassive = false;

    if (Orbwalker::ActiveMode() != OrbwalkingMode::Combo) {
        return;
    }
    if (!Bool(ComboMenu, "useE") || !E.IsReady()) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    const Vector3 dashTarget = player.Position().Extend(Game::CursorPos(), 700.0f);
    if (player.CountEnemyHeroesInRange(700.0f) <= 1) {
        E.Cast(dashTarget);
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling()) {
        return;
    }
    if (Game::IsChatOpen() || player.Spellbook().IsWindingUp() || player.IsDashing()) {
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

    AutoHarass();
}

static void Combo() {
    if (!ShouldRunNow(LastComboEvalTick, 60)) {
        return;
    }

    if (Bool(ComboMenu, "useQ") && Q.IsReady()) {
        if (!HasPassive) {
            const auto target = GetTarget(Q.Range, DamageType::Physical);
            if (ValidHeroTarget(target, Q.Range)) {
                Q.CastOnUnit(target);
            } else {
                const auto extended = GetTarget(QExtended.Range, DamageType::Physical);
                if (ValidHeroTarget(extended, QExtended.Range)) {
                    CastExtendedQThroughMinion(extended);
                }
            }
        } else {
            for (const auto& enemy : GameObjects::EnemyHeroes()) {
                if (ValidHeroTarget(enemy, Q.Range) && IsKillable(enemy, Q.GetDamage(enemy))) {
                    Q.CastOnUnit(enemy);
                    break;
                }
            }
        }
    }

    if (Bool(ComboMenu, "useW") && W.IsReady()) {
        if (!HasPassive) {
            // Nếu có địch trong tầm đánh thường → dùng biến thể không collision.
            bool anyInAaRange = false;
            for (const auto& enemy : GameObjects::EnemyHeroes()) {
                if (ValidHeroTarget(enemy) && AutoAttack::InAutoAttackRange(enemy)) {
                    anyInAaRange = true;
                    break;
                }
            }
            if (anyInAaRange) {
                const auto target = GetTarget(W.Range, DamageType::Physical);
                if (ValidHeroTarget(target, W.Range)) {
                    const auto pred = WNoCollision.GetPrediction(target, true);
                    if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                        WNoCollision.Cast(pred.GetCastPosition());
                    }
                }
            } else {
                const auto target = GetTargetNoCollision(W);
                if (ValidHeroTarget(target, W.Range)) {
                    W.Cast(target);
                }
            }
        } else {
            for (const auto& enemy : GameObjects::EnemyHeroes()) {
                if (ValidHeroTarget(enemy, W.Range) && IsKillable(enemy, W.GetDamage(enemy))) {
                    const auto pred = W.GetPrediction(enemy, true);
                    if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                        W.Cast(pred.GetCastPosition());
                        break;
                    }
                }
            }
        }
    }
}

static void Mixed() {
    const auto player = Player();
    if (!player.IsValid() || !ManaOkay(Slider(HarassMenu, "Mana", 0)) || HasPassive) {
        return;
    }

    if (Bool(HarassMenu, "useQ") && Q.IsReady()) {
        const auto target = GetTarget(Q.Range, DamageType::Physical);
        if (ValidHeroTarget(target, Q.Range)) {
            Q.CastOnUnit(target);
        } else {
            const auto extended = GetTarget(QExtended.Range, DamageType::Physical);
            if (ValidHeroTarget(extended, QExtended.Range)) {
                CastExtendedQThroughMinion(extended);
            }
        }
    }

    if (Bool(HarassMenu, "useW") && W.IsReady()) {
        const auto target = GetTargetNoCollision(W);
        if (ValidHeroTarget(target, W.Range)) {
            W.Cast(target);
        }
    }
}

static void Clear() {
    const auto player = Player();
    if (!player.IsValid() || HasPassive) {
        return;
    }

    // Lane clear Q: xuyên qua cụm >=3 lính.
    if (Bool(LaneClearMenu, "useQ", false) && Q.IsReady() && ManaOkay(Slider(LaneClearMenu, "Mana", 60))) {
        const Vector3 playerServer = player.ServerPosition();
        auto minions = GameObjects::EnemyLaneMinions();
        for (const auto& minion : minions) {
            if (!ValidTarget(minion, Q.Range)) {
                continue;
            }
            const Vector3 boxEnd = playerServer.Extend(minion.ServerPosition(), QExtended.Range);
            SDK::RectanglePoly box(playerServer, boxEnd, QExtended.Width);
            int hit = 0;
            for (const auto& other : minions) {
                if (ValidTarget(other, QExtended.Range) && box.IsInside(other.ServerPosition())) {
                    ++hit;
                }
            }
            if (hit >= 3) {
                Q.CastOnUnit(AIBaseClient(minion.Handle()));
                break;
            }
        }
    }

    // Jungle clear: mob máu cao nhất trong tầm.
    auto mobs = GameObjects::Jungle();
    mobs.erase(
        std::remove_if(
            mobs.begin(),
            mobs.end(),
            [](const AIMinionClient& mob) {
                return !ValidTarget(mob, 600.0f) || mob.IsPlant() || mob.IsPet();
            }),
        mobs.end());
    std::sort(
        mobs.begin(),
        mobs.end(),
        [](const AIMinionClient& a, const AIMinionClient& b) {
            return a.MaxHealth() > b.MaxHealth();
        });
    if (mobs.empty()) {
        return;
    }
    const auto& mob = mobs.front();

    if (Bool(JungleClearMenu, "useQ") && Q.IsReady() && ManaOkay(Slider(JungleClearMenu, "Mana", 20)) &&
        ValidTarget(mob, Q.Range)) {
        Q.CastOnUnit(mob);
    }
    if (Bool(JungleClearMenu, "useW") && W.IsReady() && ManaOkay(Slider(JungleClearMenu, "Mana", 20)) &&
        ValidTarget(mob, W.Range)) {
        W.Cast(mob.Position());
    }
}

static void AutoHarass() {
    if (!Bool(HarassMenu, "autoHarass") || !Bool(HarassMenu, "useQ") || !Q.IsReady()) {
        return;
    }
    const OrbwalkingMode mode = Orbwalker::ActiveMode();
    if (mode == OrbwalkingMode::Combo || mode == OrbwalkingMode::Harass) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid() || player.IsRecalling() || !ManaOkay(Slider(HarassMenu, "Mana", 0))) {
        return;
    }

    const auto extended = GetTarget(QExtended.Range, DamageType::Physical);
    if (ValidHeroTarget(extended, QExtended.Range)) {
        // Không kéo địch dưới trụ về phía mình đang đứng trụ.
        if (player.IsUnderEnemyTurret() && extended.IsUnderEnemyTurret()) {
            return;
        }
        CastExtendedQThroughMinion(extended);
    }
}

static void Gapcloser_OnGapcloser(const GapCloserEventArgs& args) {
    if (!Bool(MiscMenu, "gapcloser") || !E.IsReady()) {
        return;
    }
    const auto sender = AIHeroClient(args.Sender);
    if (!ValidHeroTarget(sender)) {
        return;
    }
    const std::string name = sender.CharacterName();
    if (_stricmp(name.c_str(), "MasterYi") == 0) {
        return;
    }
    const auto player = Player();
    if (player.IsValid() && args.End.Distance2D(player.Position()) <= 200.0f) {
        E.Cast(player.Position().Extend(sender.Position(), -E.Range));
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnProcessSpell -= &OnProcessSpell;
    Events::hook.OnPlayAnimation -= &OnPlayAnimation;
    Events::hook.OnBuffRemove -= &OnBuffRemove;
    Orbwalker::OnBeforeAttack -= &OnBeforeAttack;
    Orbwalker::OnAfterAttack -= &OnAfterAttack;
    Events::hook.OnGapCloser -= &Gapcloser_OnGapcloser;

    Loaded = false;
}

} // namespace Plugins::SharpAIO::Lucian
