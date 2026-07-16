#pragma once

// ============================================================================
// SharpShooter AIO — Jhin
// Port từ CSharpFiles/Jhin/Jhin.cs (ImpulseAIO) sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h + SharpShooterAIO/Corki.h.
//
// Kỹ năng:
//   Q Dancing Grenade   — targeted 550, delay 0.25, missile 1800; nảy 3 lần.
//   W Deadly Flourish   — skillshot line 2520, delay 0.75, width 45, hitscan, collision.
//   E Captive Audience  — bẫy circle 750, delay 0.25, radius 160, speed 1600.
//   R Curtain Call      — channel skillshot line 3400, delay 0.25, width 80, speed 5000;
//                         4 phát, mỗi phát scaling theo % máu mất; cone 60° từ điểm cast.
//
// Ghi chú port (giữ 1-1 với C#):
//   * State machine R: IsCastingR / Stacks / TapKeyPressed, đọc R.Instance().Name()
//     ("JhinR" = đang ngắm, "JhinRShot" = vừa bắn 1 phát).
//   * Auto: AutoW lên địch dính E-buff bất động (per-enemy toggle), AutoE lên địch
//     bất động / đang cast important spell.
//   * Flower (Captive Audience / "Noxious Trap") tracking qua OnCreate/OnDelete để
//     tránh đặt bẫy E chồng bẫy sẵn có.
//   * Damage tính tay theo wiki (patch V26.09) — KHÔNG dùng Spell::GetDamage.
// ============================================================================

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace Plugins::SharpAIO::Jhin {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* UltimateMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* LastHitMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* MiscMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 550.0f };
inline Spell W{ SpellSlot::W, 2520.0f };
inline Spell E{ SpellSlot::E, 750.0f };
inline Spell R{ SpellSlot::R, 3400.0f };

inline bool Loaded = false;

// State machine R + passive.
inline bool IsCastingR = false;
inline bool IsCharging = false;
inline bool TapKeyPressed = false;
inline int Stacks = 4;
inline int LastRShotTick = 0;
inline int RChannelStartTick = 0;

// Cone R1 lưu tại thời điểm bắt đầu ngắm R (thay Geometry.Sector của C#).
inline Vector3 RConeOrigin{};
inline Vector3 RConeDir{};   // hướng chuẩn hoá 2D (x,z) từ origin tới cursor.

// Flower (Captive Audience / Noxious Trap) tracking.
struct FlowerInfo {
    AIBaseClient Pointer;
    int ValidTime = 0;
};
inline std::vector<FlowerInfo> FlowersInfo;

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

static bool ValidUnit(const AttackableUnit& unit) {
    return unit.IsValid() && !unit.IsDead() && unit.Health() > 0.0f;
}

static bool ValidTarget(const AIBaseClient& unit, float range = FLT_MAX) {
    return ValidUnit(unit) && Extensions::IsValidTarget(unit, range, true);
}

static bool ValidHeroTarget(const AIHeroClient& hero, float range = FLT_MAX) {
    return ValidUnit(hero) && Extensions::IsValidTarget(hero, range, true);
}

static AIHeroClient GetTarget(float range, DamageType damageType) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTarget(range, damageType) : AIHeroClient();
}

static bool HitchanceAtLeast(HitChance actual, HitChance needed) {
    return static_cast<int>(actual) >= static_cast<int>(needed);
}

// C#: R.Instance.SData.Name == "JhinR" (đang ngắm, chưa bắn phát nào).
static bool IsR1() {
    return R.Instance().Name() == "JhinR";
}

// C#: target.HasBuff("jhinespotteddebuff") — dính bẫy E.
static bool TargetHaveEBuff(const AIBaseClient& target) {
    return target.IsValid() && target.HasBuff("jhinespotteddebuff");
}

// C#: Player.HasBuff("jhinpassiveattackbuff") — đang có phát AA đặc biệt.
static bool HavePassiveAttackBuff() {
    const auto player = Player();
    return player.IsValid() && player.HasBuff("jhinpassiveattackbuff");
}

// Q range tính thêm bounding radius (như C# GetQRange).
static float GetQRange(const AIBaseClient& unit) {
    const auto player = Player();
    if (!player.IsValid()) {
        return Q.Range;
    }
    if (!unit.IsValid()) {
        return Q.Range + player.BoundingRadius();
    }
    return Q.Range + player.BoundingRadius() + unit.BoundingRadius();
}

// ── Damage tính tay theo wiki (leagueoflegends.com/Jhin, patch V26.09) ──
// KHÔNG dùng Spell::GetDamage. Chốt số ngày 2026-07-08.
//
// Q Dancing Grenade (PHYSICAL): 44/69/94/119/144 + (44/51.5/59/66.5/74%) AD + 60% AP
// W Deadly Flourish (PHYSICAL): 70/105/140/175/210 + 50% AD (lính -25%)
// E Captive Audience(MAGIC)   : 20/80/140/200/260 + 120% AD + 100% AP
// R Curtain Call    (PHYSICAL): mỗi phát min 64/128/192 + 25% AD,
//                               scaling 0..300% theo % máu MẤT của mục tiêu
//                               → max 256/512/768 + 100% AD.
static float SpellDamage(SpellSlot slot, const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }
    const float totalAd = player.AD();
    const float ap = player.AP();
    const bool minion = target.IsMinion() && !target.IsHero();

    switch (slot) {
    case SpellSlot::Q: {
        const int rank = Q.Instance().Level();
        if (rank < 1) {
            return 0.0f;
        }
        static const float base[5] = { 44.0f, 69.0f, 94.0f, 119.0f, 144.0f };
        static const float adRatio[5] = { 0.44f, 0.515f, 0.59f, 0.665f, 0.74f };
        const float raw = base[rank - 1] + adRatio[rank - 1] * totalAd + 0.60f * ap;
        return Damage::CalculateDamage(player, target, DamageType::Physical, raw);
    }
    case SpellSlot::W: {
        const int rank = W.Instance().Level();
        if (rank < 1) {
            return 0.0f;
        }
        static const float base[5] = { 70.0f, 105.0f, 140.0f, 175.0f, 210.0f };
        float raw = base[rank - 1] + 0.50f * totalAd;
        if (minion) {
            raw *= 0.75f;
        }
        return Damage::CalculateDamage(player, target, DamageType::Physical, raw);
    }
    case SpellSlot::E: {
        const int rank = E.Instance().Level();
        if (rank < 1) {
            return 0.0f;
        }
        static const float base[5] = { 20.0f, 80.0f, 140.0f, 200.0f, 260.0f };
        const float raw = base[rank - 1] + 1.20f * totalAd + 1.00f * ap;
        return Damage::CalculateDamage(player, target, DamageType::Magical, raw);
    }
    case SpellSlot::R: {
        const int rank = R.Instance().Level();
        if (rank < 1) {
            return 0.0f;
        }
        const int idx = (rank - 1 < 3) ? rank - 1 : 2;
        static const float minBase[3] = { 64.0f, 128.0f, 192.0f };
        const float missingFrac = target.MaxHealth() > 0.0f
            ? std::clamp((target.MaxHealth() - target.Health()) / target.MaxHealth(), 0.0f, 1.0f)
            : 0.0f;
        // min * (1 + 3*missing) → tại full HP = min, tại missing 100% = max (x4).
        const float raw = (minBase[idx] + 0.25f * totalAd) * (1.0f + 3.0f * missingFrac);
        return Damage::CalculateDamage(player, target, DamageType::Physical, raw);
    }
    default:
        return 0.0f;
    }
}

// Có bẫy Jhin đã đặt gần vị trí pos không (để không đặt E chồng).
static bool FlowerNear(const Vector3& pos) {
    for (const auto& flower : FlowersInfo) {
        if (flower.Pointer.IsValid() && flower.Pointer.Distance(pos) <= E.Width) {
            return true;
        }
    }
    return false;
}

// C#: CastE(pos) — bỏ qua nếu đã có bẫy sẵn gần đó.
static void CastE(const Vector3& pos) {
    if (FlowerNear(pos)) {
        return;
    }
    E.Cast(pos);
}

// Enemy có nằm trong cone R1 (60°, tầm R.Range) tính từ điểm bắt đầu ngắm không.
static bool InsideRCone(const Vector3& position) {
    if (!position.IsValid()) {
        return false;
    }
    const float dx = position.x - RConeOrigin.x;
    const float dz = position.z - RConeOrigin.z;
    const float dist = std::sqrt(dx * dx + dz * dz);
    if (dist <= 0.01f || dist > R.Range) {
        return false;
    }
    const float nx = dx / dist;
    const float nz = dz / dist;
    const float cosAngle = nx * RConeDir.x + nz * RConeDir.z;
    // Nửa góc cone = 30° → cos(30°) ≈ 0.86603.
    return cosAngle >= 0.86603f;
}

static bool InsideRCone(const AIBaseClient& enemy) {
    return enemy.IsValid() && InsideRCone(enemy.Position());
}

// Forward declarations — đúng thứ tự file C#.
static void Auto();
static void Draw();
static void PermaActive();
static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void AntiGapCloser(const GapCloserEventArgs& args);
static void OnProcessSpellCast(const Events::ProcessSpellEventArgs& args);
static void OnObjectCreate(const GameObject& obj);
static void OnObjectDelete(const GameObject& obj);
static void CastW(const AIBaseClient& target, bool checkWFlag = false);
static void Combo();
static AIBaseClient GetMinionsBestQ(const AIBaseClient& unit);
static void Harass();
static void LaneClear();
static void JungleClear();
static void LastHit();
static void CastR();
static void CastQ(const AIBaseClient& unit);
static void Killsteal();
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Jhin", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("CQ", "Use Q"));
    ComboMenu->Add(new MenuList("CW", "Use W",
        std::vector<std::string>{ "Disable", "Only W/E Buff", "Always" }, 1));

    UltimateMenu = MenuRoot->AddSubMenu(new Menu("Ultimate Settings", "Ultimate"));
    UltimateMenu->Add(new MenuList("Mode", "R Aim Mode",
        std::vector<std::string>{ "Disable", "Use Key", "Auto" }, 2));
    UltimateMenu->Add(new MenuBool("OnlyKillable", "Only Killable"));
    UltimateMenu->Add(new MenuSlider("Delay", "R Delay(ms)", 0, 0, 1500));
    UltimateMenu->Add(new MenuBool("NearMouse.Enabled", "OnlyNearMouse enemy", false));
    UltimateMenu->Add(new MenuSlider("NearMouse.Radius", "NearMouse Radius", 500, 100, 1500));
    UltimateMenu->Add(new MenuBool("NearMouse.Draw", "Draw Radius"));
    UltimateMenu->Add(new MenuKeyBind("RKey", "R Tap Key", 'T', KeyBindType::Press));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("Q", "Use Q"));
    HarassMenu->Add(new MenuBool("HQPoke", "Use Q Minion Poke"));
    HarassMenu->Add(new MenuBool("W", "Use W", false));
    HarassMenu->Add(new MenuSlider("ManaPercent", "Don't Harass if Mana <= X%", 20, 0, 100));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear Settings", "LaneClear"));
    LaneClearMenu->Add(new MenuBool("Q", "Use Q"));
    LaneClearMenu->Add(new MenuSlider("MinQ", "Use Q Min hit Minion", 3, 1, 5));
    LaneClearMenu->Add(new MenuSlider("LaneClear.ManaPercent", "Don't Lane/Jungle if Mana <= X%", 50, 0, 100));

    LastHitMenu = MenuRoot->AddSubMenu(new Menu("LastHit Settings", "LastHit"));
    LastHitMenu->Add(new MenuBool("LastHit.Q", "Use Q"));
    LastHitMenu->Add(new MenuSlider("LastHit.ManaPercent", "Don't LastHit if Mana <= X%", 50, 0, 100));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("JungleClear Settings", "JungleClear"));
    JungleClearMenu->Add(new MenuBool("JungleClear.Q", "Use Q"));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    MiscMenu->Add(new MenuBool("W.AutoEnable", "Auto W To E Buff enemies"));
    MiscMenu->Add(new MenuSlider("W.ManaPercent", "Don't AutoW if Mana <= X%", 10, 0, 100));
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        const std::string name = enemy.CharacterName();
        if (name.empty()) {
            continue;
        }
        const std::string key = "AutoW." + name;
        MiscMenu->Add(new MenuBool(key.c_str(), name.c_str()));
    }
    MiscMenu->Add(new MenuBool("Misc.Immobile", "Auto E CC"));
    MiscMenu->Add(new MenuBool("E.Gapcloser", "Use E AntiGap"));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 550.0f);
    Q.SetTargetted(0.25f, 1800.0f);
    Q.DamageType = DamageType::Physical;

    W = Spell(SpellSlot::W, 2520.0f);
    W.SetSkillshot(0.75f, 45.0f, FLT_MAX, true, SpellType::Line);
    W.SetCollisionObjects(
        SDK::CollisionableObjects::Heroes |
        SDK::CollisionableObjects::YasuoWall);
    W.DamageType = DamageType::Physical;

    E = Spell(SpellSlot::E, 750.0f);
    E.SetSkillshot(0.25f, 160.0f, 1600.0f, false, SpellType::Circle);
    E.DamageType = DamageType::Physical;

    R = Spell(SpellSlot::R, 3400.0f);
    R.SetSkillshot(0.25f, 80.0f, 5000.0f, true, SpellType::Line);
    R.SetCollisionObjects(
        SDK::CollisionableObjects::Heroes |
        SDK::CollisionableObjects::YasuoWall);
    R.DamageType = DamageType::Physical;

    BuildMenu();

    // Nạp bẫy đã tồn tại sẵn (giống InitFlowersArray).
    for (const auto& obj : GameObjects::AllGameObjects()) {
        OnObjectCreate(obj);
    }

    Events::hook.OnDoCast += &OnProcessSpellCast;
    Events::hook.OnGapCloser += &AntiGapCloser;
    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Drawing::OnDraw += &Draw;
    GameObjects::AddOnCreate(&OnObjectCreate);
    GameObjects::AddOnDelete(&OnObjectDelete);

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Jhin loaded</font>");
}

static void OnObjectCreate(const GameObject& obj) {
    const auto base = AIBaseClient(obj.Handle());
    if (!base.IsValid()) {
        return;
    }
    // C#: Name == "Noxious Trap" && MaxHealth == 6 && IsAlly.
    const std::string name = ToLower(RuntimeObjectName(obj));
    if ((name.find("noxious") != std::string::npos || name.find("jhin") != std::string::npos) &&
        name.find("trap") != std::string::npos && base.MaxHealth() <= 6.0f && base.IsAlly()) {
        const int id = base.NetworkId();
        if (std::any_of(FlowersInfo.begin(), FlowersInfo.end(),
            [id](const FlowerInfo& flower) {
                return flower.Pointer.IsValid() && flower.Pointer.NetworkId() == id;
            })) {
            return;
        }
        FlowerInfo info;
        info.Pointer = base;
        info.ValidTime = SDK::Variables::TickCount() + 180000;
        FlowersInfo.push_back(info);
    }
}

static void OnObjectDelete(const GameObject& obj) {
    const int netId = obj.NetworkId();
    FlowersInfo.erase(
        std::remove_if(
            FlowersInfo.begin(),
            FlowersInfo.end(),
            [netId](const FlowerInfo& f) {
                return f.Pointer.IsValid() && f.Pointer.NetworkId() == netId;
            }),
        FlowersInfo.end());
}

static void Auto() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (Bool(MiscMenu, "W.AutoEnable")) {
        if (W.IsReady() && player.ManaPercent() >= static_cast<float>(Slider(MiscMenu, "W.ManaPercent", 10)) &&
            !player.IsUnderEnemyTurret() && player.CountEnemyHeroesInRange(600.0f) == 0) {
            for (const auto& enemy : GameObjects::EnemyHeroes()) {
                if (!ValidHeroTarget(enemy, W.Range) || !TargetHaveEBuff(enemy)) {
                    continue;
                }
                const std::string key = "AutoW." + enemy.CharacterName();
                if (!Bool(MiscMenu, key.c_str(), false)) {
                    continue;
                }
                const auto pred = W.GetPrediction(enemy);
                if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                    W.Cast(pred.GetCastPosition());
                    break;
                }
            }
        }
    }

    if (Bool(MiscMenu, "Misc.Immobile")) {
        if (E.IsReady()) {
            for (const auto& enemy : GameObjects::EnemyHeroes()) {
                if (!ValidHeroTarget(enemy, E.Range)) {
                    continue;
                }
                const auto pred = E.GetPrediction(enemy);
                if (HitchanceAtLeast(pred.Hitchance, HitChance::Immobile) ||
                    (Extensions::IsCastingInterruptableSpell(enemy, true) && !SDK::CanMove(enemy))) {
                    CastE(enemy.PreviousPosition());
                }
            }
        }
    }
}

static void Draw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (Bool(UltimateMenu, "NearMouse.Enabled", false) &&
        Bool(UltimateMenu, "NearMouse.Draw", false) && IsCastingR) {
        Drawing::DrawCircle(Game::CursorPos(),
            static_cast<float>(Slider(UltimateMenu, "NearMouse.Radius", 500)), 0xFF0000FFu);
    }
    if (R.IsReady() || IsCastingR) {
        for (const auto& enemy : GameObjects::EnemyHeroes()) {
            if (ValidHeroTarget(enemy, R.Range) &&
                enemy.Health() < SpellDamage(SpellSlot::R, enemy)) {
                Drawing::DrawText(enemy.Position(), "R Killable", 0xFFFF0000u, true);
            }
        }
    }
}

static void PermaActive() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (IsCastingR) {
        // C#: IsCastingR = R.Instance.Name == "JhinRShot" (đang trong channel bắn).
        const int elapsed = SDK::Variables::TickCount() - RChannelStartTick;
        IsCastingR = player.Spellbook().IsChanneling() || elapsed < 700;
    }
    IsCharging = player.HasBuff("JhinPassiveReload");
    Orbwalker::AttackEnabled(!IsCastingR);
    Orbwalker::MoveEnabled(!IsCastingR);
    if (R.IsReady() && !IsCastingR) {
        Stacks = 4;
    }

    if (IsCastingR) {
        // "Use Key" mode (index 1): giữ phím RKey để bơm TapKeyPressed. "Auto" (2)
        // tự bắn. C#: if (TapKeyPressed || Mode == 2) CastR().
        const int mode = ListIndex(UltimateMenu, "Mode", 2);
        const auto* rKey = UltimateMenu ? UltimateMenu->Get<MenuKeyBind>("RKey") : nullptr;
        if (mode == 1 && rKey && rKey->Active) {
            TapKeyPressed = true;
        }
        if (TapKeyPressed || mode == 2) {
            CastR();
        }
        return;
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling()) {
        return;
    }
    if (Game::IsChatOpen()) {
        return;
    }

    Killsteal();
    PermaActive();
    Auto();

    switch (Orbwalker::ActiveMode()) {
    case OrbwalkingMode::Combo:
        Combo();
        break;
    case OrbwalkingMode::Harass:
        Harass();
        break;
    case OrbwalkingMode::LaneClear:
        LaneClear();
        JungleClear();
        break;
    case OrbwalkingMode::LastHit:
        LastHit();
        break;
    default:
        break;
    }
}

static void AntiGapCloser(const GapCloserEventArgs& args) {
    if (!Bool(MiscMenu, "E.Gapcloser")) {
        return;
    }
    const auto sender = AIHeroClient(args.Sender);
    if (!ValidHeroTarget(sender)) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (player.Position().Distance(args.Start) > player.Position().Distance(args.End)) {
        if (args.End.Distance(player.Position()) <= E.Range) {
            CastE(args.End);
            return;
        }
    }

    if (TargetHaveEBuff(sender) && W.IsReady()) {
        if (player.CountEnemyHeroesInRange(650.0f) == 0) {
            if (args.End.Distance(player.Position()) <= E.Range) {
                CastW(sender);
            }
        }
    }
}

static void OnProcessSpellCast(const Events::ProcessSpellEventArgs& args) {
    if (!Events::IsLocalPlayer(args.Sender)) {
        return;
    }
    if (args.Slot != static_cast<int>(SpellSlot::R)) {
        return;
    }
    if (std::strcmp(args.SpellName, "JhinR") == 0) {
        IsCastingR = true;
        RChannelStartTick = SDK::Variables::TickCount();
        const auto player = Player();
        if (player.IsValid()) {
            RConeOrigin = player.PreviousPosition();
            const Vector3 toCursor = RConeOrigin.Extend(Game::CursorPos(), R.Range);
            const float dx = toCursor.x - RConeOrigin.x;
            const float dz = toCursor.z - RConeOrigin.z;
            const float len = std::sqrt(dx * dx + dz * dz);
            if (len > 0.01f) {
                RConeDir = Vector3(dx / len, 0.0f, dz / len);
            }
        }
        Stacks = 4;
    } else if (std::strcmp(args.SpellName, "JhinRShot") == 0) {
        LastRShotTick = SDK::Variables::TickCount();
        TapKeyPressed = false;
        --Stacks;
    }
}

static void CastW(const AIBaseClient& target, bool checkWFlag) {
    if (IsCastingR) {
        return;
    }
    if (!W.IsReady() || !target.IsValid()) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (checkWFlag && ListIndex(ComboMenu, "CW", 1) == 1) {
        if (!TargetHaveEBuff(target)) {
            return;
        }
    }
    if (player.CountEnemyHeroesInRange(500.0f) != 0) {
        return;
    }
    for (const auto& h : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(h) && h.IsMelee() && AutoAttack::InAutoAttackRange(h)) {
            return;
        }
    }
    if (AutoAttack::InAutoAttackRange(target) && Orbwalker::CanAttack() &&
        target.Health() < Damage::GetAutoAttackDamage(player, target)) {
        return;
    }
    if (target.IsHero()) {
        const auto hero = AIHeroClient(target.Handle());
        if (hero.IsValid()) {
            if (!TargetHaveEBuff(hero)) {
                if (Orbwalker::CanAttack() && AutoAttack::InAutoAttackRange(target)) {
                    return;
                }
            }
        }
    }
    const auto pred = W.GetPrediction(target);
    if (HitchanceAtLeast(pred.Hitchance, HitChance::High) && pred.CollisionObjects.empty()) {
        W.Cast(pred.GetCastPosition());
    }
}

static void Combo() {
    const auto player = Player();
    if (!player.IsValid() || player.Spellbook().IsWindingUp() || IsCastingR) {
        return;
    }
    if (Q.IsReady() && Bool(ComboMenu, "CQ") && !HavePassiveAttackBuff()) {
        const auto target = GetTarget(Q.Range, DamageType::Physical);
        if (ValidHeroTarget(target)) {
            CastQ(AIBaseClient(target.Handle()));
        }
    }
    if (W.IsReady() && ListIndex(ComboMenu, "CW", 1) != 0) {
        for (const auto& obj : GameObjects::EnemyHeroes()) {
            if (ValidHeroTarget(obj, W.Range)) {
                CastW(AIBaseClient(obj.Handle()), true);
            }
        }
    }
}

// C#: bắn Q vào lính có thể giết & gần địch (nảy 3 lần tới hero).
static AIBaseClient GetMinionsBestQ(const AIBaseClient& unit) {
    AIBaseClient best;
    if (!unit.IsValid()) {
        return best;
    }

    std::vector<AIBaseClient> allMinions;
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (ValidTarget(minion, GetQRange(minion))) {
            allMinions.push_back(AIBaseClient(minion.Handle()));
        }
    }

    std::vector<AIBaseClient> minionUp;
    for (const auto& m : allMinions) {
        if (SpellDamage(SpellSlot::Q, m) > m.Health() && m.Distance(unit) <= 225.0f) {
            minionUp.push_back(m);
        }
    }

    for (const auto& seed : minionUp) {
        std::vector<AIBaseClient> minDist;
        for (const auto& m : allMinions) {
            if (m.NetworkId() != seed.NetworkId()) {
                minDist.push_back(m);
            }
        }
        std::sort(minDist.begin(), minDist.end(),
            [&seed](const AIBaseClient& a, const AIBaseClient& b) {
                return a.Distance(seed) < b.Distance(seed);
            });
        for (int j = 0; j < static_cast<int>(minDist.size()); ++j) {
            if (j >= 3) {
                break;
            }
            if (minDist[j].NetworkId() == unit.NetworkId()) {
                best = seed;
            }
        }
    }
    return best;
}

static void Harass() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (player.ManaPercent() <= static_cast<float>(Slider(HarassMenu, "ManaPercent", 20))) {
        return;
    }
    if (IsCastingR) {
        return;
    }
    if (Bool(HarassMenu, "Q") && Q.IsReady() && !HavePassiveAttackBuff()) {
        const auto ret = GetTarget(Q.Range, DamageType::Physical);
        if (ValidHeroTarget(ret)) {
            const auto retBase = AIBaseClient(ret.Handle());
            if (ret.DistanceToPlayer() <= GetQRange(retBase)) {
                if (Bool(HarassMenu, "HQPoke")) {
                    const auto bestQ = GetMinionsBestQ(retBase);
                    if (bestQ.IsValid()) {
                        CastQ(bestQ);
                        return;
                    }
                    CastQ(retBase);
                } else {
                    CastQ(retBase);
                    return;
                }
            } else if (Bool(HarassMenu, "HQPoke")) {
                const auto bestQ = GetMinionsBestQ(retBase);
                if (bestQ.IsValid()) {
                    CastQ(bestQ);
                    return;
                }
            }
        }
    }
    if (Bool(HarassMenu, "W", false) && W.IsReady()) {
        for (const auto& obj : GameObjects::EnemyHeroes()) {
            if (ValidHeroTarget(obj, W.Range)) {
                CastW(AIBaseClient(obj.Handle()));
            }
        }
    }
}

static void LaneClear() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (player.ManaPercent() <= static_cast<float>(Slider(LaneClearMenu, "LaneClear.ManaPercent", 50))) {
        return;
    }

    if (Bool(LaneClearMenu, "Q") && Q.IsReady()) {
        const int minHits = Slider(LaneClearMenu, "MinQ", 3);
        // Snapshot 1 lần: GameObjects::EnemyMinions() copy cả vector dưới mutex mỗi
        // lần gọi, nên nested loop gốc trả 1 copy đầy đủ mỗi vòng trong (O(n^2) copy).
        // List là snapshot cache đông cứng nên dùng lại là behavior-identical.
        const auto laneMinions = GameObjects::EnemyMinions();
        for (const auto& minion : laneMinions) {
            if (!ValidTarget(minion, Q.Range) ||
                minion.Health() >= SpellDamage(SpellSlot::Q, AIBaseClient(minion.Handle()))) {
                continue;
            }
            int nearby = 0;
            for (const auto& other : laneMinions) {
                if (ValidTarget(other) && other.Distance(minion) <= 225.0f) {
                    ++nearby;
                }
            }
            if (nearby >= minHits) {
                CastQ(AIBaseClient(minion.Handle()));
                return;
            }
        }
    }
}

static void JungleClear() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (player.ManaPercent() <= static_cast<float>(Slider(LaneClearMenu, "LaneClear.ManaPercent", 50))) {
        return;
    }

    if (Bool(JungleClearMenu, "JungleClear.Q") && Q.IsReady()) {
        AIMinionClient best;
        float bestHp = FLT_MAX;
        for (const auto& mob : GameObjects::Jungle()) {
            if (ValidTarget(mob, Q.Range) && !mob.IsPlant() && !mob.IsPet() && mob.Health() < bestHp) {
                bestHp = mob.Health();
                best = mob;
            }
        }
        if (best.IsValid()) {
            CastQ(AIBaseClient(best.Handle()));
        }
    }
}

static void LastHit() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (player.ManaPercent() < static_cast<float>(Slider(LastHitMenu, "LastHit.ManaPercent", 50))) {
        return;
    }

    if (Bool(LastHitMenu, "LastHit.Q") && Q.IsReady()) {
        AIMinionClient best;
        float bestMaxHp = -1.0f;
        for (const auto& minion : GameObjects::EnemyMinions()) {
            const auto minionBase = AIBaseClient(minion.Handle());
            if (!ValidTarget(minion, GetQRange(minionBase))) {
                continue;
            }
            if (!(SpellDamage(SpellSlot::Q, minionBase) > minion.Health())) {
                continue;
            }
            const bool underAlly = minion.IsUnderAllyTurret();
            const bool underEnemy = minion.IsUnderEnemyTurret();
            const bool farFromAa = minion.DistanceToPlayer() > AutoAttack::GetRealAutoAttackRange(minion) + 50.0f;
            const bool tanky = minion.Health() > Damage::GetAutoAttackDamage(player, minion);
            if (!(underAlly || (underEnemy && !player.IsUnderEnemyTurret()) || farFromAa || tanky)) {
                continue;
            }
            if (minion.MaxHealth() > bestMaxHp) {
                bestMaxHp = minion.MaxHealth();
                best = minion;
            }
        }
        if (best.IsValid()) {
            CastQ(AIBaseClient(best.Handle()));
        }
    }
}

static void CastR() {
    if (IsR1()) {
        return;
    }
    if (SDK::Variables::TickCount() - LastRShotTick < Slider(UltimateMenu, "Delay", 0)) {
        return;
    }

    const bool onlyKillable = Bool(UltimateMenu, "OnlyKillable", false);
    const bool nearMouse = Bool(UltimateMenu, "NearMouse.Enabled", false);
    const float nearRadius = static_cast<float>(Slider(UltimateMenu, "NearMouse.Radius", 500));

    AIHeroClient best;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy, R.Range) || !InsideRCone(enemy)) {
            continue;
        }
        if (onlyKillable && !(enemy.Health() < SpellDamage(SpellSlot::R, enemy))) {
            continue;
        }
        if (nearMouse && enemy.Position().Distance(Game::CursorPos()) > nearRadius) {
            continue;
        }
        if (!best.IsValid() || enemy.Health() < best.Health()) {
            best = enemy;
        }
    }

    if (best.IsValid()) {
        const auto pred = R.GetPrediction(best);
        const Vector3 castPos = pred.GetCastPosition();
        if (HitchanceAtLeast(pred.Hitchance, HitChance::VeryHigh) &&
            pred.CollisionObjects.empty() && InsideRCone(castPos)) {
            const auto player = Player();
            if (player.IsValid() && player.Spellbook().CastSpell(SpellSlot::R, castPos)) {
                LastRShotTick = SDK::Variables::TickCount();
            }
        }
    }
}

static void CastQ(const AIBaseClient& unit) {
    if (IsCastingR) {
        return;
    }
    Q.Cast(unit);
}

static void Killsteal() {
    if (IsCastingR) {
        return;
    }

    for (const auto& obj : GameObjects::EnemyHeroes()) {
        if (!ValidUnit(obj)) {
            continue;
        }
        if (obj.HasBuff("BansheesVeil") || obj.HasBuff("SivirShield") || obj.HasBuff("ShroudofDarkness")) {
            continue;
        }
        const auto objBase = AIBaseClient(obj.Handle());
        if (ValidTarget(objBase, GetQRange(objBase)) &&
            obj.Health() < SpellDamage(SpellSlot::Q, objBase) && Q.IsReady()) {
            CastQ(objBase);
        }
        if (ValidTarget(objBase, W.Range) &&
            obj.Health() < SpellDamage(SpellSlot::W, objBase) && W.IsReady()) {
            CastW(objBase);
        }
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnDoCast -= &OnProcessSpellCast;
    Events::hook.OnGapCloser -= &AntiGapCloser;
    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Drawing::OnDraw -= &Draw;
    GameObjects::RemoveOnCreate(&OnObjectCreate);
    GameObjects::RemoveOnDelete(&OnObjectDelete);

    FlowersInfo.clear();
    Loaded = false;
}

} // namespace Plugins::SharpAIO::Jhin
