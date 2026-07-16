#pragma once

// ============================================================================
// SharpShooter AIO — Samira
// Port từ CSharpFiles/Samira/Samira.cs (ImpulseAIO) sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h + SharpShooterAIO/Corki.h.
//
// Kỹ năng:
//   Q Flair          — cone 375 (khi target sát) / line 950 (khi target xa),
//                      delay 0.25. ConeQ width 45, LineQ width 60 speed 2600 collision.
//   W Blade Whirl     — no-target 390, chém quanh Samira, chặn đạn.
//   E Wild Rush       — dash targeted 600 tới địch/lính (E xong auto ConeQ).
//   R Inferno Trigger — no-target 600, chỉ dùng khi đủ style-grade (xem ghi chú).
//
// Ghi chú port (giữ 1-1 tới mức API cho phép):
//   * LastSpell tracking + CanCastThisSpell: giữ nguyên (đảm bảo xen kẽ spell/AA
//     để không tự huỷ passive).
//   * W anti-projectile evade (EvadeTarget): port gọn qua MissileClient — chặn đạn
//     hướng về mình khi có trong tầm W.
//   * MISSING API (không chế, xem missapi.md):
//       - R.Instance.IconUsed (style-grade meter Non..S) → không có trong SDK.
//         → GetPassiveStack() đọc style qua buff "samiradaredevilimpulse" count
//           làm proxy; điều kiện R giữ theo C# (enemy count >= X hoặc killable).
//       - Packet mode (Game.OnProcessPacket header 56) → bỏ, cast thường.
//       - Player.IssueOrder(MoveTo) → CoreControl::IssueMove (khi có buff SamiraR).
//   * Damage tính tay theo wiki (patch V26.x) — KHÔNG dùng Spell::GetDamage.
// ============================================================================

#include "../../../SDK/SDK.h"
#include "../../../core/CoreControl.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace Plugins::SharpAIO::Samira {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* LastHitMenu = nullptr;
inline Menu* DrawMenu = nullptr;
inline Menu* EvadeMenu = nullptr;

inline Spell ConeQ{ SpellSlot::Q, 375.0f };
inline Spell LineQ{ SpellSlot::Q, 950.0f };
inline Spell W{ SpellSlot::W, 390.0f };
inline Spell E{ SpellSlot::E, 600.0f };
inline Spell R{ SpellSlot::R, 600.0f };

inline bool Loaded = false;
inline SpellSlot LastSpell = SpellSlot::Unknown;
inline float LastAaMs = 0.0f;
inline int WaitAaaTick = 0;
inline int WaitCastETick = 0;
inline int LastRCastTick = 0;

// C# Passive enum (style-grade).
enum class Passive {
    Non = 0,
    E = 2,
    D = 3,
    C = 4,
    B = 5,
    A = 6,
    S = 7,
};

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

// C# AfterAttack: sau đòn AA (đã qua ~70% attack delay) thì được xen spell.
static bool AfterAttack() {
    const auto player = Player();
    if (!player.IsValid()) {
        return false;
    }
    const float attackEndMs = LastAaMs + SDK::AttackDelay(AIBaseClient(player.Handle())) * 1000.0f;
    const float nowMs = Game::Time() * 1000.0f;
    const float remain = attackEndMs - nowMs;
    return remain >= remain * 0.7f;
}

// MISSING API: R.Instance.IconUsed. Proxy qua stack buff passive.
static Passive GetPassiveStack() {
    const auto player = Player();
    if (!player.IsValid()) {
        return Passive::Non;
    }
    const int raw = player.GetBuffCount("samiradaredevilimpulse");
    if (raw >= 7) return Passive::S;
    if (raw >= 6) return Passive::A;
    if (raw >= 5) return Passive::B;
    if (raw >= 4) return Passive::C;
    if (raw >= 3) return Passive::D;
    if (raw >= 2) return Passive::E;
    return Passive::Non;
}

// C#: R học rồi & cooldown <= 2s coi như sẵn sàng.
static bool CheckRReady() {
    if (!R.Instance().Learned()) {
        return false;
    }
    return R.Instance().RemainingCooldown(Game::Time()) <= 2.0f;
}

static bool TryCastR() {
    const auto player = Player();
    const int now = SDK::Variables::TickCount();
    if (!player.IsValid() || GetPassiveStack() != Passive::S || !R.IsReady() ||
        player.HasBuff("SamiraR") || now - LastRCastTick < 3000) {
        return false;
    }
    if (R.Cast()) {
        LastRCastTick = now;
        return true;
    }
    return false;
}

// ── Damage tính tay theo wiki (leagueoflegends.com/Samira, patch V26.x) ──
// KHÔNG dùng Spell::GetDamage. Chốt số ngày 2026-07-08. Samira full-AD, no AP.
//
// Q Flair (PHYSICAL): 0/5/10/15/20 + 110% total AD.
// E Wild Rush (MAGIC): 50/60/70/80/90 + 20% bonus AD.
// R Inferno Trigger (PHYSICAL, tổng 10 phát): 200/400/600 + 300% total AD (lính -75%).
static float GetQDmg(const AIBaseClient& w) {
    const auto player = Player();
    if (!player.IsValid() || !w.IsValid()) {
        return 0.0f;
    }
    const int level = ConeQ.Instance().Level();
    if (level < 1) {
        return 0.0f;
    }
    const float raw = 5.0f * static_cast<float>(level - 1) +
        (0.8f + 0.1f * static_cast<float>(level - 1)) * player.AD();
    return Damage::CalculateDamage(player, w, DamageType::Physical, raw);
}

static float GetRDmg(const AIBaseClient& w) {
    const auto player = Player();
    if (!player.IsValid() || !w.IsValid()) {
        return 0.0f;
    }
    const int level = ConeQ.Instance().Level();
    const float raw = 10.0f * static_cast<float>(level - 1) + 0.5f * player.AD() * 10.0f;
    return Damage::CalculateDamage(player, w, DamageType::Physical, raw);
}

// C# GetComboDamage: Q + E + (R nếu includeR) + 2 đòn AA.
static float GetComboDamage(const AIBaseClient& w, bool includeR) {
    const auto player = Player();
    if (!player.IsValid() || !w.IsValid()) {
        return 0.0f;
    }
    float physicalRaw = 0.0f;
    float magicRaw = 0.0f;
    if (ConeQ.IsReady()) {
        const int ql = ConeQ.Instance().Level();
        physicalRaw += 5.0f * static_cast<float>(ql - 1) +
            (0.8f + 0.1f * static_cast<float>(ql - 1)) * player.AD();
    }
    if (E.IsReady()) {
        const int el = E.Instance().Level();
        magicRaw += 50.0f + 10.0f * static_cast<float>(el - 1) + 0.2f * player.BonusAttackDamage();
    }
    if (includeR && GetPassiveStack() >= Passive::C && (CheckRReady() || player.HasBuff("SamiraR"))) {
        const int rl = ConeQ.Instance().Level();
        physicalRaw += 10.0f * static_cast<float>(rl - 1) + 0.5f * player.AD() * 10.0f;
    }
    physicalRaw += Damage::GetAutoAttackDamage(player, w) * 2.0f;
    const float phys = Damage::CalculateDamage(player, w, DamageType::Physical, physicalRaw);
    const float magic = Damage::CalculateDamage(player, w, DamageType::Magical, magicRaw);
    return phys + magic;
}

// C#: vị trí sau khi E vào target.
static Vector3 PosAfterE(const AIBaseClient& w) {
    const auto player = Player();
    if (!player.IsValid() || !w.IsValid()) {
        return {};
    }
    Vector3 pos = player.PreviousPosition().Extend(w.PreviousPosition(), E.Range);
    pos.y = NavMesh::GetHeightForPosition(pos);
    return pos;
}

static bool HasAnyHeroInWRange() {
    const auto player = Player();
    if (!player.IsValid()) {
        return false;
    }
    for (const auto& x : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(x) &&
            x.PreviousPosition().Distance(player.PreviousPosition()) <= W.Range + x.BoundingRadius()) {
            return true;
        }
    }
    return false;
}

// C# CanCastThisSpell: đảm bảo không lặp lại cùng 1 spell liên tiếp (giữ passive).
static bool CanCastThisSpell(SpellSlot w) {
    if (LastSpell == SpellSlot::Unknown) {
        return false;
    }
    if (w == SpellSlot::Item1) {          // Item1 = auto-attack marker trong C#.
        return LastSpell != w;
    }
    if (w != SpellSlot::Item1 && LastSpell != w && Extensions::IsReady(w)) {
        return true;
    }
    return false;
}

// Forward declarations — đúng thứ tự file C#.
static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnProcessSpellCastOnLastSpell(const Events::ProcessSpellEventArgs& args);
static void OnDraw();
static void OnCastSpell(const Events::ProcessSpellEventArgs& args);
static void Check();
static void LaneClear();
static void LastHit();
static void JungleClear();
static void Harass();
static void Combo();
static AIBaseClient GetSamiraBestETarget();
static void CastEKey();
static void EvadeUpdate();
static void OnMissileCreate(const GameObject& obj);
static void OnMissileDelete(const GameObject& obj);
static void OnUnload();

// Danh sách đạn nguy hiểm đang bay tới mình (EvadeTarget rút gọn).
struct DangerMissile {
    int NetworkId = 0;
    Vector3 Position;
};
inline std::vector<DangerMissile> DangerMissiles;

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Samira", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("CQ", "Use Q"));
    ComboMenu->Add(new MenuBool("CW", "Use W"));
    ComboMenu->Add(new MenuBool("CWOnlyHaveR", "Use W Only have R", false));
    ComboMenu->Add(new MenuBool("CE", "Use E"));
    ComboMenu->Add(new MenuKeyBind("CEUnderTower", "Use E UnderTower", 'A', KeyBindType::Toggle));
    ComboMenu->Add(new MenuKeyBind("Ekey", "Fast E to MousePos", 'T', KeyBindType::Press));
    ComboMenu->Add(new MenuBool("CR", "Use R"));
    ComboMenu->Add(new MenuSlider("CRCount", "Count enemy >= X", 2, 1, 5));
    ComboMenu->Add(new MenuBool("CROnlyKill", "killable R"));

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("HQ", "Use Q"));
    HarassMenu->Add(new MenuSlider("HQMagic", "Don't Harass if mana <= X%", 30, 0, 100));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear Settings", "LaneClear"));
    LaneClearMenu->Add(new MenuBool("LQ", "Use Q"));
    LaneClearMenu->Add(new MenuSlider("LQMagic", "Don't lane/jungle if mana <= X%", 30, 0, 100));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("JungleClear Settings", "JungleClear"));
    JungleClearMenu->Add(new MenuBool("JQ", "Use Q"));

    LastHitMenu = MenuRoot->AddSubMenu(new Menu("LastHit Settings", "LastHit"));
    LastHitMenu->Add(new MenuBool("LhQ", "Use Q"));
    LastHitMenu->Add(new MenuSlider("LhQMagic", "Don't lasthit if mana <= X%", 20, 0, 100));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Draw Settings", "Draw"));
    DrawMenu->Add(new MenuBool("DQ", "Draw Q"));
    DrawMenu->Add(new MenuBool("DE", "Draw E"));
    DrawMenu->Add(new MenuBool("DR", "Draw R"));

    EvadeMenu = MenuRoot->AddSubMenu(new Menu("Evade Settings", "W Evade"));
    EvadeMenu->Add(new MenuBool("W", "Use W to block projectiles"));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    ConeQ = Spell(SpellSlot::Q, 375.0f);
    ConeQ.SetSkillshot(0.25f, 45.0f, FLT_MAX, false, SpellType::Cone);
    ConeQ.DamageType = DamageType::Physical;
    ConeQ.MinHitChance = HitChance::High;

    LineQ = Spell(SpellSlot::Q, 950.0f);
    LineQ.SetSkillshot(0.25f, 60.0f, 2600.0f, true, SpellType::Line);
    LineQ.SetCollisionObjects(
        SDK::CollisionableObjects::Minions |
        SDK::CollisionableObjects::YasuoWall);
    LineQ.DamageType = DamageType::Physical;
    LineQ.MinHitChance = HitChance::High;

    W = Spell(SpellSlot::W, 390.0f);

    E = Spell(SpellSlot::E, 600.0f);
    E.SetTargetted(0.0f, 500.0f);

    R = Spell(SpellSlot::R, 600.0f);

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnProcessSpell += &OnProcessSpellCastOnLastSpell;
    Events::hook.OnProcessSpell += &OnCastSpell;
    Drawing::OnDraw += &OnDraw;
    GameObjects::AddOnCreate(&OnMissileCreate);
    GameObjects::AddOnDelete(&OnMissileDelete);

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Samira loaded</font>");
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (Game::IsChatOpen()) {
        return;
    }

    Check();
    CastEKey();
    EvadeUpdate();

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

static void OnProcessSpellCastOnLastSpell(const Events::ProcessSpellEventArgs& args) {
    if (!Events::IsLocalPlayer(args.Sender)) {
        return;
    }
    switch (static_cast<SpellSlot>(args.Slot)) {
    case SpellSlot::Q:
        LastSpell = SpellSlot::Q;
        break;
    case SpellSlot::W:
        LastSpell = SpellSlot::W;
        break;
    case SpellSlot::E:
        Game::SendEmote(EmoteId::Joke);
        LastSpell = SpellSlot::E;
        break;
    case SpellSlot::R:
        Game::SendEmote(EmoteId::Joke);
        LastSpell = SpellSlot::R;
        LastRCastTick = SDK::Variables::TickCount();
        break;
    default:
        if (args.IsAutoAttack) {
            LastAaMs = Game::Time() * 1000.0f;
            LastSpell = SpellSlot::Item1;
            if (GetPassiveStack() == Passive::Non) {
                WaitAaaTick = SDK::Variables::TickCount() + 150;
            }
        }
        break;
    }
}

static void OnCastSpell(const Events::ProcessSpellEventArgs& args) {
    if (!Events::IsLocalPlayer(args.Sender)) {
        return;
    }
    // C#: sau khi E vào hero → ConeQ tại chỗ (auto Q sau dash).
    if (args.Slot == static_cast<int>(SpellSlot::E)) {
        const auto player = Player();
        if (player.IsValid() && ConeQ.IsReady()) {
            ConeQ.Cast(player.PreviousPosition());
        }
    }
}

static void OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (Bool(DrawMenu, "DQ", false) && LineQ.IsReady()) {
        Drawing::DrawCircle(player.Position(), LineQ.Range, 0xFF00FF00u);
    }
    if (Bool(DrawMenu, "DE", false) && E.IsReady()) {
        Drawing::DrawCircle(player.Position(), E.Range, 0xFFFFA500u);
    }
    if (Bool(DrawMenu, "DR", false) && CheckRReady()) {
        Drawing::DrawCircle(player.Position(), R.Range, 0xFFFF0000u);
    }
}

static void Check() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (GetPassiveStack() == Passive::Non && !AfterAttack()) {
        LastSpell = SpellSlot::Unknown;
    }
    // Khoá AA khi đang W / channel / R.
    Orbwalker::AttackEnabled(
        !(player.HasBuff("SamiraW") || player.Spellbook().IsWindingUp() || player.HasBuff("SamiraR")));
    // Đang R (Inferno Trigger) → giữ di chuyển về con trỏ.
    if (player.HasBuff("SamiraR")) {
        CoreControl::IssueMove(Game::CursorPos(), true);
    }
}

static void LaneClear() {
    const auto player = Player();
    if (!player.IsValid() || player.ManaPercent() <= static_cast<float>(Slider(LaneClearMenu, "LQMagic", 30))) {
        return;
    }
    if (Bool(LaneClearMenu, "LQ") && ConeQ.IsReady() && !player.Spellbook().IsWindingUp()) {
        std::vector<AIBaseClient> minions;
        for (const auto& m : GameObjects::EnemyMinions()) {
            if (ValidTarget(m, ConeQ.Range)) {
                minions.push_back(AIBaseClient(m.Handle()));
            }
        }
        if (!minions.empty()) {
            const auto farm = ConeQ.GetCircularFarmLocation(minions);
            if (farm.MinionsHit >= 2) {
                ConeQ.Cast(Vector3::From2D(farm.Position));
                return;
            }
        }

        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (!ValidTarget(minion, LineQ.Range)) {
                continue;
            }
            const float hp = HealthPrediction(AIBaseClient(minion.Handle()), 500);
            if (!(hp > 0.0f && hp <= GetQDmg(AIBaseClient(minion.Handle())))) {
                continue;
            }
            const bool underAlly = minion.IsUnderAllyTurret();
            const bool underEnemy = minion.IsUnderEnemyTurret();
            const bool farFromAa = minion.DistanceToPlayer() > AutoAttack::GetRealAutoAttackRange(minion) + 50.0f;
            const bool tanky = minion.Health() > Damage::GetAutoAttackDamage(player, minion);
            if (!(underAlly || (underEnemy && !player.IsUnderEnemyTurret()) || farFromAa || tanky)) {
                continue;
            }
            const auto pred = LineQ.GetPrediction(minion);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::High) && pred.CollisionObjects.empty() &&
                pred.GetCastPosition().Distance(player.Position()) <= LineQ.Range) {
                LineQ.Cast(pred.GetCastPosition());
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
    if (Bool(LastHitMenu, "LhQ") && player.ManaPercent() > static_cast<float>(Slider(LastHitMenu, "LhQMagic", 20))) {
        AIMinionClient best;
        float bestMaxHp = -1.0f;
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (!ValidTarget(minion, LineQ.Range)) {
                continue;
            }
            const float hp = HealthPrediction(AIBaseClient(minion.Handle()), 500);
            if (!(hp > 0.0f && hp <= GetQDmg(AIBaseClient(minion.Handle())))) {
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
            const auto pred = LineQ.GetPrediction(best);
            if (HitchanceAtLeast(pred.Hitchance, HitChance::Medium)) {
                LineQ.Cast(pred.GetCastPosition());
            }
        }
    }
}

static void JungleClear() {
    const auto player = Player();
    if (!player.IsValid() || player.ManaPercent() <= static_cast<float>(Slider(LaneClearMenu, "LQMagic", 30))) {
        return;
    }
    if (Bool(JungleClearMenu, "JQ") && ConeQ.IsReady()) {
        for (const auto& mob : GameObjects::Jungle()) {
            if (ValidTarget(mob, LineQ.Range) && LineQ.IsReady()) {
                LineQ.Cast(mob.Position());
            }
        }
    }
}

static void Harass() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (Bool(HarassMenu, "HQ") && static_cast<float>(Slider(HarassMenu, "HQMagic", 30)) < player.ManaPercent()) {
        if (LineQ.IsReady() && !player.Spellbook().IsWindingUp()) {
            AIHeroClient bestTarget;
            HitChance bestHc = HitChance::None;
            Vector3 bestPos;
            for (const auto& t : GameObjects::EnemyHeroes()) {
                if (!ValidHeroTarget(t, LineQ.Range)) {
                    continue;
                }
                const auto pred = LineQ.GetPrediction(t);
                if (HitchanceAtLeast(pred.Hitchance, HitChance::High) && pred.CollisionObjects.empty() &&
                    pred.GetCastPosition().Distance(player.Position()) <= LineQ.Range) {
                    if (static_cast<int>(pred.Hitchance) > static_cast<int>(bestHc)) {
                        bestHc = pred.Hitchance;
                        bestPos = pred.GetCastPosition();
                        bestTarget = t;
                    }
                }
            }
            if (bestTarget.IsValid()) {
                LineQ.Cast(bestPos);
            }
        }
    }
}

static void Combo() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    if (player.Spellbook().IsWindingUp() && CanCastThisSpell(SpellSlot::Item1)) {
        return;
    }

    // W: chỉ khi passive chưa S và điều kiện xen kẽ spell hợp lệ (giữ 1-1).
    if (Bool(ComboMenu, "CW") && W.IsReady() && CanCastThisSpell(SpellSlot::W) &&
        (!Bool(ComboMenu, "CWOnlyHaveR", false) || CheckRReady())) {
        if (GetPassiveStack() != Passive::S &&
            ((GetPassiveStack() == Passive::A && CanCastThisSpell(SpellSlot::W) && CheckRReady() && !CanCastThisSpell(SpellSlot::Item1)) ||
             (GetPassiveStack() > Passive::Non && LineQ.IsReady() && E.IsReady()) ||
             (GetPassiveStack() == Passive::B && !LineQ.IsReady() && !CanCastThisSpell(SpellSlot::Item1)) ||
             (GetPassiveStack() >= Passive::D && CanCastThisSpell(SpellSlot::E)))) {
            if (W.IsReady() && player.Mana() > E.Instance().ManaCost() + W.Instance().ManaCost()) {
                if (HasAnyHeroInWRange()) {
                    W.Cast();
                    WaitCastETick = SDK::Variables::TickCount() + 100;
                }
            }
        }
    }

    // E: dash tới best target (khi không phải "còn LineQ mà passive Non").
    if (Bool(ComboMenu, "CE") && E.IsReady()) {
        if (!(LineQ.IsReady() && GetPassiveStack() == Passive::Non)) {
            if (SDK::Variables::TickCount() > WaitCastETick && CanCastThisSpell(SpellSlot::E)) {
                const auto target = GetSamiraBestETarget();
                if (target.IsValid() && ValidTarget(target)) {
                    E.CastOnUnit(target);
                }
            }
        }
    }

    // Q: cone nếu target sát, line nếu xa; gate bởi AfterAttack/LastSpell.
    if (Bool(ComboMenu, "CQ") && LineQ.IsReady() && !player.IsDashing() &&
        SDK::Variables::TickCount() > WaitAaaTick) {
        const auto targetCone = GetTarget(ConeQ.Range, DamageType::Physical);
        if (ValidHeroTarget(targetCone)) {
            if (CanCastThisSpell(SpellSlot::Q) &&
                (AfterAttack() || LastSpell == SpellSlot::Item1 ||
                 (GetPassiveStack() == Passive::A && LastSpell == SpellSlot::E))) {
                const auto pred = Prediction::GetPrediction(AIBaseClient(targetCone.Handle()), 0.25f);
                if (pred.GetCastPosition().IsValid()) {
                    ConeQ.Cast(pred.GetCastPosition());
                }
            }
        } else {
            const auto targetLine = GetTarget(LineQ.Range, DamageType::Physical);
            if (ValidHeroTarget(targetLine)) {
                if (CanCastThisSpell(SpellSlot::Q) &&
                    (AfterAttack() || LastSpell == SpellSlot::Item1 ||
                     (GetPassiveStack() == Passive::A && LastSpell == SpellSlot::E))) {
                    const auto pred = LineQ.GetPrediction(targetLine);
                    if (HitchanceAtLeast(pred.Hitchance, HitChance::High)) {
                        LineQ.Cast(pred.GetCastPosition());
                    }
                }
            }
        }
    }

    // R: đủ style S + đủ số địch quanh, hoặc killable.
    if (Bool(ComboMenu, "CR") && GetPassiveStack() == Passive::S) {
        bool shouldCast = player.CountEnemyHeroesInRange(R.Range) >= Slider(ComboMenu, "CRCount", 2);
        if (!shouldCast && Bool(ComboMenu, "CROnlyKill", false)) {
            for (const auto& x : GameObjects::EnemyHeroes()) {
                if (ValidHeroTarget(x, R.Range) && x.Health() < GetRDmg(AIBaseClient(x.Handle()))) {
                    shouldCast = true;
                    break;
                }
            }
        }
        if (shouldCast) {
            TryCastR();
        }
    }
}

// C# GetSamiraBestETarget rút gọn: chọn hero HP thấp nhất trong tầm E (+LineQ),
// ưu tiên có thể giết bằng combo hoặc dash vào tầm AA (tránh越塔 khi CEUnderTower off).
static AIBaseClient GetSamiraBestETarget() {
    const auto player = Player();
    if (!player.IsValid()) {
        return AIBaseClient();
    }
    const bool underTowerOk = KeyActive(ComboMenu, "CEUnderTower");

    std::vector<AIHeroClient> heroes;
    for (const auto& x : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(x, E.Range + LineQ.Range)) {
            heroes.push_back(x);
        }
    }
    std::sort(heroes.begin(), heroes.end(),
        [](const AIHeroClient& a, const AIHeroClient& b) {
            return a.Health() < b.Health();
        });

    for (const auto& killObj : heroes) {
        const auto killBase = AIBaseClient(killObj.Handle());
        const Vector3 posAfter = PosAfterE(killBase);
        if (!underTowerOk && killObj.IsUnderEnemyTurret()) {
            continue;
        }
        // E thẳng vào target nếu trong tầm E và combo hạ gục / vào tầm AA.
        if (E.Range >= player.PreviousPosition().Distance(killObj.PreviousPosition())) {
            const bool killable = killObj.Health() <= GetComboDamage(killBase, true);
            const bool intoAaRange = posAfter.IsValid() &&
                posAfter.Distance(killObj.PreviousPosition()) <= AutoAttack::GetRealAutoAttackRange(killObj);
            if (killable || intoAaRange) {
                return killBase;
            }
        }
    }
    return AIBaseClient();
}

// C# castekey: E nhanh tới target gần con trỏ nhất (đưa mình lại gần cursor).
static void CastEKey() {
    if (!KeyActive(ComboMenu, "Ekey")) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    AIBaseClient best;
    float bestCursorDist = FLT_MAX;
    const float playerCursorDist = player.Position().Distance(Game::CursorPos());
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy, E.Range)) {
            continue;
        }
        const Vector3 posAfter = PosAfterE(AIBaseClient(enemy.Handle()));
        if (!posAfter.IsValid()) {
            continue;
        }
        const float cd = posAfter.Distance(Game::CursorPos());
        if (cd < playerCursorDist && cd < bestCursorDist) {
            bestCursorDist = cd;
            best = AIBaseClient(enemy.Handle());
        }
    }
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (!ValidTarget(minion, E.Range)) {
            continue;
        }
        const Vector3 posAfter = PosAfterE(AIBaseClient(minion.Handle()));
        if (!posAfter.IsValid()) {
            continue;
        }
        const float cd = posAfter.Distance(Game::CursorPos());
        if (cd < playerCursorDist && cd < bestCursorDist) {
            bestCursorDist = cd;
            best = AIBaseClient(minion.Handle());
        }
    }
    if (best.IsValid()) {
        E.CastOnUnit(best);
    }
}

// W-evade: nếu có đạn nguy hiểm bay tới trong tầm W → chém W chặn.
static void EvadeUpdate() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (!Bool(EvadeMenu, "W", true) || !W.IsReady()) {
        return;
    }
    for (const auto& missile : DangerMissiles) {
        if (missile.Position.Distance(player.Position()) < 700.0f &&
            missile.Position.Distance(player.Position()) <= W.Range) {
            W.Cast();
            return;
        }
    }
}

// C# ObjSpellMissileOnCreate rút gọn: đạn enemy hero hướng về mình.
static void OnMissileCreate(const GameObject& obj) {
    const auto missile = MissileClient(obj.Handle());
    if (!missile.IsValid()) {
        return;
    }
    const int casterNet = missile.CasterNetworkId();
    if (casterNet == 0) {
        return;
    }
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    bool fromEnemyHero = false;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (enemy.NetworkId() == casterNet) {
            fromEnemyHero = true;
            break;
        }
    }
    if (!fromEnemyHero) {
        return;
    }
    if (missile.TargetNetworkId() != 0 && missile.TargetNetworkId() != player.NetworkId()) {
        return;
    }
    DangerMissile dm;
    dm.NetworkId = missile.NetworkId();
    dm.Position = missile.EndPosition();
    DangerMissiles.push_back(dm);
}

static void OnMissileDelete(const GameObject& obj) {
    const auto missile = MissileClient(obj.Handle());
    if (!missile.IsValid()) {
        return;
    }
    const int netId = missile.NetworkId();
    DangerMissiles.erase(
        std::remove_if(
            DangerMissiles.begin(),
            DangerMissiles.end(),
            [netId](const DangerMissile& m) { return m.NetworkId == netId; }),
        DangerMissiles.end());
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnProcessSpell -= &OnProcessSpellCastOnLastSpell;
    Events::hook.OnProcessSpell -= &OnCastSpell;
    Drawing::OnDraw -= &OnDraw;
    GameObjects::RemoveOnCreate(&OnMissileCreate);
    GameObjects::RemoveOnDelete(&OnMissileDelete);

    DangerMissiles.clear();
    Loaded = false;
}

} // namespace Plugins::SharpAIO::Samira
