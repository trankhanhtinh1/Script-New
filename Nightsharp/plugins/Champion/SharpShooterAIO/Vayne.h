#pragma once

// ============================================================================
// SharpShooter AIO — Vayne
// Port từ SharpShooterCSHarp/Plugins/Vayne.cs sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h.
//
// Kỹ năng:
//   Q Tumble       — dash ngắn + tăng sát thương đòn kế. Cast tới con trỏ sau đòn
//                    đánh (reposition) khi ít địch quanh; anti-melee; auto khi R.
//   W Silver Bolts — passive (không tự cast; mốc 2 stack để tính combo damage).
//   E Condemn      — targeted 700. Chỉ cast khi cú hất văng đập target vào tường
//                    (extend vị trí predict ra sau, kiểm tra IsWall).
//   R Final Hour   — buff; khi cast R tự Tumble tới con trỏ (tuỳ chọn).
//
// Ghi chú port:
//   * Wall-check dùng SDK::NavMesh::IsWall (tương đương .IsWall() của C#).
//   * Tumble-stealth: chặn auto-attack khi buff "vaynetumblefade" còn đủ lâu để
//     giữ tàng hình (đọc GetStartTime/GetEndTime của buff qua CoreBuffs).
// ============================================================================

#include "../../../SDK/SDK.h"
#include "../../../core/CoreBuffs.h"

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

namespace Plugins::SharpAIO::Vayne {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* MiscMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 915.0f };
inline Spell W{ SpellSlot::W, FLT_MAX };
inline Spell E{ SpellSlot::E, 700.0f };
inline Spell R{ SpellSlot::R, FLT_MAX };

inline bool Loaded = false;

// Combo AA -> Q -> AA: OnAfterAttack fire tại cast-START của đòn AA (windup chưa
// xong, missile chưa launch). Cast Q ngay đó sẽ hủy đòn AA -> mất sát thương.
// Nên ta chỉ ghi "muốn Tumble sau đòn này", rồi cast Q trong Game_OnUpdate KHI
// Orbwalker::CanMove() == true (windup đã trôi qua = đòn AA đã chắc chắn ra).
// vaynetumble reset attack -> sau Q gọi ResetAutoAttackTimer() để AA ra ngay.
inline bool s_pendingTumble = false;
inline Vector3 s_pendingTumblePos{};
inline DWORD s_pendingTumbleTick = 0;

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

static bool ManaOkay(int percent) {
    const auto player = Player();
    return player.IsValid() && player.ManaPercent() >= static_cast<float>(percent);
}

static bool ValidUnit(const AttackableUnit& unit) {
    return unit.IsValid() && !unit.IsDead() && unit.Health() > 0.0f;
}

static bool ValidHeroTarget(const AIHeroClient& hero, float range = FLT_MAX) {
    return ValidUnit(hero) && Extensions::IsValidTarget(hero, range, true);
}

static bool HitchanceAtLeast(HitChance actual, HitChance needed) {
    return static_cast<int>(actual) >= static_cast<int>(needed);
}

static AIHeroClient HeroFromInfo(const ::Core::Events::ObjectInfo& info) {
    ::Core::Objects::ObjectHandle handle{};
    handle.address = info.Ptr;
    handle.index = info.Index;
    handle.networkId = info.NetworkId;
    handle.type = info.Type;
    return AIHeroClient(handle);
}

// E Condemn: cú hất văng target ra xa mình; đập tường nếu điểm đằng sau là wall.
// Trả true nếu cast được (target sẽ đập tường trong khoảng 400 đơn vị).
static bool TryCondemn(const AIHeroClient& enemy) {
    if (!E.IsReady() || !ValidHeroTarget(enemy, E.Range)) {
        return false;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return false;
    }
    const auto pred = E.GetPrediction(enemy);
    if (!HitchanceAtLeast(pred.Hitchance, HitChance::VeryHigh)) {
        return false;
    }
    const Vector3 unitPos = pred.GetUnitPosition();

    // Điểm cách target 400 ra sau (hướng ngược mình): nếu là tường → chắc chắn đập.
    const Vector3 finalPos = unitPos.Extend(player.Position(), -400.0f);
    if (SDK::NavMesh::IsWall(finalPos)) {
        return E.CastOnUnit(AIBaseClient(enemy.Handle()));
    }
    // Quét dần 1..400: điểm nào chạm tường thì cast.
    for (int i = 1; i < 400; i += 50) {
        const Vector3 sample = unitPos.Extend(player.Position(), -static_cast<float>(i));
        if (SDK::NavMesh::IsWall(sample)) {
            return E.CastOnUnit(AIBaseClient(enemy.Handle()));
        }
    }
    return false;
}

// Killsteal: loại buff bất tử phổ biến rồi so máu + shield với damage tính được.
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

// ── Damage tính tay theo wiki (wiki.leagueoflegends.com) — KHÔNG dùng DamageData ──
// E Condemn (physical):
//     base      : 50/85/120/155/190      + 50%  bonus AD
//     wall-slam : 75/127.5/180/232.5/285 + 75%  bonus AD (bonus khi đập tường)
//     total     : 125/212.5/300/.../475  + 125% bonus AD
//   → Killsteal chỉ dùng base (KHÔNG cộng wall-slam) để tránh over-estimate:
//     E cast targeted không đảm bảo đập tường, nên chỉ tính phần chắc chắn có.
// W Silver Bolts (true, đòn thứ 3): 6/7/8/9/10% max HP; sàn 50/65/80/95/110;
//     vs quái cố định 140/155/170/185/200. W là passive on-hit — KHÔNG tự cast
//     được nên KHÔNG phải spell killsteal; chỉ ghi chú số liệu ở đây.
// Q Tumble (physical, empower đòn kế): 75/85/95/105/115% AD (+ 50% AP). Không
//     phải nuke độc lập (buff đòn đánh kế), nên không tính vào killsteal.
// Trả về damage đã trừ giáp qua Damage::CalculateDamage.
static float SpellDamage(SpellSlot slot, const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }
    const float bonusAd = player.BonusAttackDamage();

    switch (slot) {
    case SpellSlot::E: {
        const int rank = E.Instance().Level();
        if (rank < 1) {
            return 0.0f;
        }
        static const float base[5] = { 50.0f, 85.0f, 120.0f, 155.0f, 190.0f };
        // Base only + 50% bonus AD — không cộng wall-slam bonus (tránh over-estimate).
        const float raw = base[rank - 1] + 0.50f * bonusAd;
        return Damage::CalculateDamage(player, target, DamageType::Physical, raw);
    }
    default:
        return 0.0f;
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void AutoKillsteal();
static void OnProcessSpell(const ProcessSpellEventArgs& args);
static void OnBeforeAttack(OrbwalkingActionArgs& args);
static void OnAfterAttack(OrbwalkingActionArgs& args);
static void Gapcloser_OnGapcloser(const GapCloserEventArgs& args);
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Vayne", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("useQ", "Use Q (reposition)"));
    ComboMenu->Add(new MenuBool("useE", "Use E (wall condemn)"));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("useQ", "Use Q", false));
    HarassMenu->Add(new MenuSlider("Mana", "If Mana > %", 60, 0, 100));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear Settings", "Lane Clear"));
    LaneClearMenu->Add(new MenuBool("useQ", "Use Q", false));
    LaneClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 60, 0, 100));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("Jungle Settings", "Jungle Clear"));
    JungleClearMenu->Add(new MenuBool("useQ", "Use Q"));
    JungleClearMenu->Add(new MenuSlider("Mana", "If Mana > %", 20, 0, 100));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    MiscMenu->Add(new MenuBool("killsteal", "Auto Killsteal (E Condemn)"));
    MiscMenu->Add(new MenuBool("gapcloser", "Anti-Gapcloser (E)", false));
    MiscMenu->Add(new MenuBool("interrupter", "Interrupter (E)"));
    MiscMenu->Add(new MenuBool("autoQonR", "Auto Q when using R"));
    MiscMenu->Add(new MenuSlider("qStealthMs", "Q Stealth duration (ms)", 1000, 0, 1000));
    MiscMenu->Add(new MenuBool("antiMelee", "Use Anti-Melee (Q)"));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 915.0f);

    W = Spell(SpellSlot::W, FLT_MAX);

    E = Spell(SpellSlot::E, 700.0f);
    E.SetTargetted(0.375f, FLT_MAX);
    E.DamageType = DamageType::Physical;

    R = Spell(SpellSlot::R, FLT_MAX);

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnProcessSpell += &OnProcessSpell;
    Orbwalker::OnBeforeAttack += &OnBeforeAttack;
    Orbwalker::OnAfterAttack += &OnAfterAttack;
    Events::hook.OnGapCloser += &Gapcloser_OnGapcloser;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Vayne loaded</font>");
}

static void OnProcessSpell(const ProcessSpellEventArgs& args) {
    if (!Loaded) {
        return;
    }

    // Tự Tumble tới con trỏ khi vừa cast R.
    if (Events::IsLocalPlayer(args.Sender)) {
        if (args.Slot == static_cast<int>(SpellSlot::R) &&
            Bool(MiscMenu, "autoQonR") && Q.IsReady()) {
            Q.Cast(Game::CursorPos());
        }
        return;
    }

    // Anti-melee Q: địch cận chiến auto-attack mình → Tumble ra sau.
    if (!Bool(MiscMenu, "antiMelee") || !Q.IsReady() || !args.IsAutoAttack || args.Target.Ptr == 0) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid() || player.NetworkId() != static_cast<int>(args.Target.NetworkId)) {
        return;
    }
    const AIHeroClient sender = HeroFromInfo(args.Sender);
    if (sender.IsValid() && sender.IsHero() && sender.IsEnemy() && sender.IsMelee()) {
        Q.Cast(player.Position().Extend(sender.Position(), -Q.Range));
    }
}

// Tumble-stealth: chặn auto-attack khi buff "vaynetumblefade" còn đủ lâu để giữ
// tàng hình (không đứng dưới trụ).
static void OnBeforeAttack(OrbwalkingActionArgs& args) {
    if (!Loaded) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    const auto buff = ::CoreBuffs::FindByName(player.Address(), "vaynetumblefade");
    if (!buff.IsValid()) {
        return;
    }
    const float now = Game::Time();
    const float endTime = buff.GetEndTime();
    const float startTime = buff.GetStartTime();
    const float stealthWindow = static_cast<float>(Slider(MiscMenu, "qStealthMs", 1000)) / 1000.0f;
    // Còn nhiều hơn (tổng-thời-lượng - cửa-sổ) → vẫn trong giai đoạn tàng hình.
    if ((endTime - now) > (endTime - startTime - stealthWindow) &&
        !player.IsUnderEnemyTurret()) {
        args.Process = false;
    }
}

// Sau đòn đánh: KHÔNG cast Q ngay (sẽ hủy đòn AA đang windup). Chỉ đánh dấu
// "muốn Tumble" theo mode; Game_OnUpdate sẽ cast Q khi đòn AA đã ra (CanMove).
static void OnAfterAttack(OrbwalkingActionArgs& args) {
    if (!Loaded || !Q.IsReady()) {
        return;
    }
    const auto targetBase = AIBaseClient(args.Target.Handle());
    if (!ValidUnit(targetBase)) {
        return;
    }

    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    const OrbwalkingMode mode = Orbwalker::ActiveMode();

    bool wantQ = false;
    if (mode == OrbwalkingMode::Combo) {
        wantQ = targetBase.IsHero() && Bool(ComboMenu, "useQ") &&
            player.CountEnemyHeroesInRange(700.0f) <= 1;
    } else if (mode == OrbwalkingMode::Harass) {
        wantQ = targetBase.IsHero() && Bool(HarassMenu, "useQ", false) &&
            ManaOkay(Slider(HarassMenu, "Mana", 60)) &&
            player.CountEnemyHeroesInRange(700.0f) <= 1;
    } else if (mode == OrbwalkingMode::LaneClear) {
        const bool lane = targetBase.IsMinion() && Bool(LaneClearMenu, "useQ", false) &&
            ManaOkay(Slider(LaneClearMenu, "Mana", 60)) &&
            player.CountEnemyHeroesInRange(700.0f) <= 1;
        const bool jungle = targetBase.IsMinion() && targetBase.Team() == GameObjectTeam::Neutral &&
            Bool(JungleClearMenu, "useQ") && ManaOkay(Slider(JungleClearMenu, "Mana", 20));
        wantQ = lane || jungle;
    }

    if (wantQ) {
        s_pendingTumble = true;
        s_pendingTumblePos = Game::CursorPos();
        s_pendingTumbleTick = GetTickCount();
    }
}

// Xử lý Tumble đang chờ: chỉ cast khi đòn AA đã ra (Orbwalker::CanMove() true =
// windup xong). Sau khi cast Q -> ResetAutoAttackTimer() để đòn AA kế ra ngay
// (vaynetumble hủy animation). Bỏ pending nếu quá hạn 600ms hoặc Q hết sẵn sàng.
static void ProcessPendingTumble() {
    if (!s_pendingTumble) {
        return;
    }
    if (!Q.IsReady() || GetTickCount() - s_pendingTumbleTick > 600) {
        s_pendingTumble = false;
        return;
    }
    // Đợi đòn AA vừa rồi thoát windup (đã chắc chắn ra) mới Tumble.
    if (!Orbwalker::CanMove()) {
        return;
    }
    Vector3 dest = s_pendingTumblePos;
    if (!dest.IsValid() || dest.IsZero()) {
        dest = Game::CursorPos();
    }
    if (Q.Cast(dest)) {
        s_pendingTumble = false;
        // Q reset AA -> cho phép AA ra ngay sau khi Tumble cast, không đợi delay.
        Orbwalker::ResetAutoAttackTimer();
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

    // Tumble đang chờ: cast Q khi đòn AA đã ra (CanMove) rồi reset AA timer.
    ProcessPendingTumble();

    // Auto killsteal: chạy mọi mode, không phụ thuộc combo.
    AutoKillsteal();

    // E Condemn wall-slam trong combo.
    if (Orbwalker::ActiveMode() == OrbwalkingMode::Combo &&
        Bool(ComboMenu, "useE") && E.IsReady()) {
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (ValidHeroTarget(enemy, E.Range) && TryCondemn(enemy)) {
                break;
            }
        }
    }
}

// Auto killsteal: E Condemn (targeted 700, physical) là spell gây damage trực
// tiếp duy nhất cast được. Dùng base damage (không tính wall-slam bonus) để
// tránh over-estimate. Tính damage tay theo wiki qua SpellDamage.
static void AutoKillsteal() {
    if (!Bool(MiscMenu, "killsteal") || !E.IsReady()) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(enemy, E.Range) && IsKillable(enemy, SpellDamage(SpellSlot::E, enemy))) {
            if (E.CastOnUnit(AIBaseClient(enemy.Handle()))) {
                return;
            }
        }
    }
}

static void Gapcloser_OnGapcloser(const GapCloserEventArgs& args) {
    if (!Bool(MiscMenu, "gapcloser", false) || !E.IsReady()) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid() || args.End.Distance2D(player.Position()) > 200.0f) {
        return;
    }
    const auto sender = AIHeroClient(args.Sender);
    if (ValidHeroTarget(sender, E.Range)) {
        E.CastOnUnit(AIBaseClient(sender.Handle()));
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnProcessSpell -= &OnProcessSpell;
    Orbwalker::OnBeforeAttack -= &OnBeforeAttack;
    Orbwalker::OnAfterAttack -= &OnAfterAttack;
    Events::hook.OnGapCloser -= &Gapcloser_OnGapcloser;

    Loaded = false;
}

} // namespace Plugins::SharpAIO::Vayne
