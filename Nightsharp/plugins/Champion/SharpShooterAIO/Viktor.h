#pragma once

// ============================================================================
// SharpShooter AIO — Viktor
// Port từ CSharpFiles/Viktor/Viktor.cs (ImpulseAIO) sang NightSharp C++.
// Kiến trúc theo chuẩn 7UPAIO/Ezreal.h + SharpShooterAIO/Orianna.h.
//
// Kỹ năng:
//   Q Siphon Power — targeted 652, empowered next AA.
//   W Gravity Field — circle 800 (delay 0.4, radius 300), CC/interrupt.
//   E Hextech Ray  — laser 2 điểm (from→to), base range 525, dài 700, tốc 1050.
//   R Arcane Storm — circle 700 (delay 0.6, radius 450), storm bám theo (follow).
//
// Ghi chú port (giữ 1-1 với C#):
//   * E laser: cast bằng Spell::Cast(from, to). Farm-location tự tìm cặp
//     (startPos,endPos) trúng nhiều minion nhất — port GetBestLaserFarmLocation 1-1.
//   * Q killsteal state: KillQTarget + thorwQWait theo dõi qua OnDoCast
//     (ViktorPowerTransfer) / OnProcessSpell (ViktorPowerTransferReturn).
//   * FollowR: R storm bám target khi R buff "ViktorChaosStorm" đang active.
//   * AutoWCC: W lên địch đang bị CC (GetCCBuffPos port inline — hero HasBuffOfType CC).
//   * Interrupter/Teleport/AntiGapcloser/NonKillableMinion hook đầy đủ.
//   * Damage tính tay theo wiki (patch V26.x) — base khớp công thức C#.
//     R tick C# KHÔNG có AP ratio; wiki mới có +35% AP/tick → giữ C# 1-1 (comment).
// ============================================================================

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include <vector>

namespace Plugins::SharpAIO::Viktor {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* ComboMenu = nullptr;
inline Menu* BlackListMenu = nullptr;
inline Menu* HarassMenu = nullptr;
inline Menu* LaneClearMenu = nullptr;
inline Menu* JungleClearMenu = nullptr;
inline Menu* KillstealMenu = nullptr;
inline Menu* MiscMenu = nullptr;
inline Menu* DrawMenu = nullptr;
inline Menu* AntiGapMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 652.0f };
inline Spell W{ SpellSlot::W, 800.0f };
inline Spell E{ SpellSlot::E, 525.0f };
inline Spell R{ SpellSlot::R, 700.0f };

inline bool Loaded = false;

// C# static consts.
inline constexpr int maxRangeE = 1200;
inline constexpr int lengthE = 700;
inline constexpr int SpeedE = 1050;
inline constexpr int rangeE = 525;

// C# state.
inline bool thorwQWait = false;
inline uint32_t KillQTargetId = 0;
inline bool KillQTargetValid = false;
inline int lasttick = 0;

struct NewFarmLocation {
    int MinionsHit = 0;
    Vector2 Position1{};
    Vector2 Position2{};
    NewFarmLocation() = default;
    NewFarmLocation(const Vector2& startpos, const Vector2& endpos, int minionsHit)
        : MinionsHit(minionsHit), Position1(startpos), Position2(endpos) {}
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

static bool HitchanceAtLeast(HitChance actual, HitChance needed) {
    return static_cast<int>(actual) >= static_cast<int>(needed);
}

static AIHeroClient TSGetTarget(float range, DamageType damageType) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTarget(range, damageType) : AIHeroClient();
}

static std::vector<AIHeroClient> TSGetTargets(float range, DamageType damageType) {
    auto* selector = SDK::TargetSelector::Instance();
    return selector ? selector->GetTargets(range, damageType) : std::vector<AIHeroClient>();
}

static float HealthPred(const AIBaseClient& u, int ms) {
    return u.IsValid() ? Prediction::Health::GetPrediction(u, ms) : 0.0f;
}

// ── Damage tính tay theo wiki (leagueoflegends.com/Viktor, patch V26.x) ──
// E Hextech Ray: 70/110/150/190/230 + 50% AP  → base = 70 + (lvl-1)*40 (khớp C#).
// R Arcane Storm initial: 100/175/250 + 50% AP → base = 100 + (lvl-1)*75.
// R tick: 65/105/145 (wiki +35% AP) — C# KHÔNG áp AP cho tick, giữ 1-1.
static float GetRDmg(const AIHeroClient& unit) {
    const auto player = Player();
    if (!player.IsValid() || !unit.IsValid()) {
        return 0.0f;
    }
    const float RBaseDmg = 100.0f + static_cast<float>(R.Instance().Level() - 1) * 75.0f;
    const float ExtraDmg = player.AP() * 0.5f;
    const float TickDmg = 65.0f + static_cast<float>(R.Instance().Level() - 1) * 40.0f;
    const float CountDmg = TickDmg * static_cast<float>(Slider(ComboMenu, "cRtick", 3));
    return player.CalculateMagicDamage(AIBaseClient(unit.Handle()), RBaseDmg + ExtraDmg + CountDmg);
}

static float GetEDmg(const AIBaseClient& unit) {
    const auto player = Player();
    if (!player.IsValid() || !unit.IsValid()) {
        return 0.0f;
    }
    const float BaseDmg = 70.0f + static_cast<float>(E.Instance().Level() - 1) * 40.0f;
    const float ExtraDmg = player.AP() * 0.5f;
    return player.CalculateMagicDamage(unit, BaseDmg + ExtraDmg);
}

// C# AttacksEnabled property.
static bool AttacksEnabled() {
    const auto player = Player();
    if (Orbwalker::ActiveMode() == OrbwalkingMode::Combo) {
        const bool qBlocked = !Q.IsReady() || player.Mana() < Q.Instance().ManaCost();
        const bool eBlocked = !E.IsReady() || player.Mana() < E.Instance().ManaCost();
        const bool aaOk = !Bool(MiscMenu, "DisableAA", false) || player.HasBuff("viktorpowertransferreturn");
        return qBlocked && eBlocked && aaOk;
    }
    return true;
}

static bool IsKillableQTarget(const AIBaseClient& unit) {
    if (!unit.IsValid() || !KillQTargetValid || !thorwQWait ||
        KillQTargetId != static_cast<uint32_t>(unit.NetworkId())) {
        return false;
    }
    return true;
}

static bool UnitIsBlock(const AIHeroClient& unit) {
    if (!BlackListMenu || !unit.IsValid()) {
        return false;
    }
    const std::string key = "blacklist." + unit.CharacterName();
    const auto* item = BlackListMenu->Get<MenuBool>(key.c_str());
    return item ? item->Value : false;
}

// C# GetCCBuffPos — vị trí địch đang bị CC (immobile) để W chụp.
static Vector3 GetCCBuffPos(const AIHeroClient& obj) {
    const auto base = AIBaseClient(obj.Handle());
    if (SDK::HasBuffOfType(base, SDK::BuffType::Stun) ||
        SDK::HasBuffOfType(base, SDK::BuffType::Snare) ||
        SDK::HasBuffOfType(base, SDK::BuffType::Charm) ||
        SDK::HasBuffOfType(base, SDK::BuffType::Taunt) ||
        SDK::HasBuffOfType(base, SDK::BuffType::Fear) ||
        SDK::HasBuffOfType(base, SDK::BuffType::Suppression) ||
        SDK::HasBuffOfType(base, SDK::BuffType::Knockup) ||
        SDK::HasBuffOfType(base, SDK::BuffType::Knockback)) {
        return obj.PreviousPosition();
    }
    return Vector3{};
}

// Forward declarations — đúng thứ tự file C#.
static void OnDoCastSpell(const Events::ProcessSpellEventArgs& args);
static void OnProcessSpell(const Events::ProcessSpellEventArgs& args);
static void OnBeforeAttack(OrbwalkingActionArgs& args);
static void OnTeleport(const Events::Teleport::TeleportEventArgs& args);
static void OnInterruptSpell(const Events::ProcessSpellEventArgs& args);
static void AntiGapCloser(const GapCloserEventArgs& args);
static void NonKillable(OrbwalkingActionArgs& args);
static void OnDraw();
static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void AutoWCCLogic();
static void FollowR();
static void AutoKill();
static NewFarmLocation GetBestLaserFarmLocation(bool jungle);
static bool PredictCastMinionE(bool jungle);
static void CastE(const Vector3& source, const Vector3& destination);
static void PredictCastE(const AIHeroClient& objs);
static void CastW(const AIHeroClient& unit);
static bool CanCastR(const AIHeroClient& unit);
static void LogicRKillable();
static void Combo();
static void Harass();
static void Laneclear();
static void JungleClear();
static void OnUnload();

static void BuildMenu() {
    MenuRoot = new Menu("champion.sharpshooteraio", "SharpAIO - Viktor", true);

    ComboMenu = MenuRoot->AddSubMenu(new Menu("Combo Settings", "Combo"));
    ComboMenu->Add(new MenuBool("CQ", "Use Q"));
    ComboMenu->Add(new MenuList("CW", "Use W",
        std::vector<std::string>{ "Always", "Only Slow/CC", "With R", "Never" }, 1));
    ComboMenu->Add(new MenuBool("CE", "Use E"));
    ComboMenu->Add(new MenuList("CR", "Use R",
        std::vector<std::string>{ "Health <= X%", "Killable", "Disable" }, 1));
    ComboMenu->Add(new MenuSlider("cRheal", "Use R if Enemy health <= X%", 40, 0, 100));
    ComboMenu->Add(new MenuSlider("cRtick", "Extra R Count", 3, 1, 6));
    ComboMenu->Add(new MenuSlider("CRCount", "R min HitCount", 3, 1, 5));
    ComboMenu->Add(new MenuBool("CRFlow", "Auto Follow R"));
    ComboMenu->Add(new MenuBool("wasteR", "Waste R"));

    BlackListMenu = MenuRoot->AddSubMenu(new Menu("R BlackList", "R BlackList"));
    for (const auto& obj : GameObjects::EnemyHeroes()) {
        const std::string name = obj.CharacterName();
        if (name.empty()) {
            continue;
        }
        const std::string key = "blacklist." + name;
        BlackListMenu->Add(new MenuBool(key.c_str(), name.c_str(), false));
    }

    HarassMenu = MenuRoot->AddSubMenu(new Menu("Harass Settings", "Harass"));
    HarassMenu->Add(new MenuBool("HQ", "Use Q"));
    HarassMenu->Add(new MenuBool("HE", "Use E"));
    HarassMenu->Add(new MenuSlider("HMana", "Don't Use Spell Harass if Mana <= X%", 60, 0, 100));

    LaneClearMenu = MenuRoot->AddSubMenu(new Menu("LaneClear Settings", "LaneClear"));
    LaneClearMenu->Add(new MenuBool("LQ", "Use Q"));
    LaneClearMenu->Add(new MenuBool("LE", "Use E"));
    LaneClearMenu->Add(new MenuSlider("LECount", "Min E HitCount", 3, 1, 6));
    LaneClearMenu->Add(new MenuSlider("LMana", "Don't LaneClear/JungleClear if Mana <= X%", 40, 0, 100));

    JungleClearMenu = MenuRoot->AddSubMenu(new Menu("JungleClear Settings", "JungleClear"));
    JungleClearMenu->Add(new MenuBool("JQ", "Use Q"));
    JungleClearMenu->Add(new MenuBool("JE", "Use E"));

    KillstealMenu = MenuRoot->AddSubMenu(new Menu("Killable Settings", "Killable"));
    KillstealMenu->Add(new MenuBool("KQ", "Use Q"));
    KillstealMenu->Add(new MenuBool("KE", "Use E"));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc Settings", "Misc"));
    MiscMenu->Add(new MenuBool("DisableAA", "Disable AA if Orb - Combo Active", false));
    MiscMenu->Add(new MenuSlider("DisableAALvL", "-> Level >= X disable AA", 12, 1, 18));
    MiscMenu->Add(new MenuBool("InterruptW", "Auto W Interrupt Spell"));
    MiscMenu->Add(new MenuBool("InterruptR", "Auto R Interrupt Spell"));
    MiscMenu->Add(new MenuBool("autoW", "Auto W CC"));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Draw Settings", "Draw"));
    DrawMenu->Add(new MenuBool("DrawQ", "Q Range", false));
    DrawMenu->Add(new MenuBool("DrawW", "W Range", false));
    DrawMenu->Add(new MenuBool("DrawE", "E Range", false));
    DrawMenu->Add(new MenuBool("DrawMaxE", "MaxE Range", false));
    DrawMenu->Add(new MenuBool("DrawR", "R Range", false));

    AntiGapMenu = MenuRoot->AddSubMenu(new Menu("AntiGap Settings", "AntiGapcloser"));
    AntiGapMenu->Add(new MenuBool("AntiEGap", "Use W AntiGapCloser"));

    MenuRoot->Attach();
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 652.0f);
    Q.SetTargetted(0.25f, 2000.0f);
    Q.DamageType = DamageType::Magical;

    W = Spell(SpellSlot::W, 800.0f);
    W.SetSkillshot(0.4f, 300.0f, FLT_MAX, false, SpellType::Circle);
    W.DamageType = DamageType::Magical;

    E = Spell(SpellSlot::E, 525.0f);
    E.SetSkillshot(0.0f, 90.0f, static_cast<float>(SpeedE), false, SpellType::Line);
    E.DamageType = DamageType::Magical;

    R = Spell(SpellSlot::R, 700.0f);
    R.SetSkillshot(0.6f, 450.0f, FLT_MAX, false, SpellType::Circle);
    R.DamageType = DamageType::Magical;

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnDoCast += &OnDoCastSpell;
    Events::hook.OnProcessSpell += &OnProcessSpell;
    Events::hook.OnProcessSpell += &OnInterruptSpell;
    Orbwalker::OnBeforeAttack += &OnBeforeAttack;
    Events::hook.OnTeleport += &OnTeleport;
    Events::hook.OnGapCloser += &AntiGapCloser;
    Orbwalker::OnNonKillableMinion += &NonKillable;
    Drawing::OnDraw += &OnDraw;

    Loaded = true;
    Game::Print("<font color='#00D8FF'>SharpAIO - Viktor loaded</font>");
}

// C#: OnDoCast — ViktorPowerTransfer (Q throw) → thorwQWait = true.
static void OnDoCastSpell(const Events::ProcessSpellEventArgs& args) {
    if (!Events::IsLocalPlayer(args.Sender)) {
        return;
    }
    if (std::string(args.SpellName) == "ViktorPowerTransfer") {
        thorwQWait = true;
    }
}

// C#: OnProcessSpell — ViktorPowerTransferReturn (Q return) → reset kill-Q state.
static void OnProcessSpell(const Events::ProcessSpellEventArgs& args) {
    if (!Events::IsLocalPlayer(args.Sender)) {
        return;
    }
    if (std::string(args.SpellName) == "ViktorPowerTransferReturn") {
        thorwQWait = false;
        KillQTargetValid = false;
        KillQTargetId = 0;
    }
}

static void OnBeforeAttack(OrbwalkingActionArgs& args) {
    if (Bool(MiscMenu, "DisableAA", false)) {
        const auto player = Player();
        const auto hero = AIHeroClient(args.Target.Handle());
        if (player.Level() >= Slider(MiscMenu, "DisableAALvL", 12) && hero.IsValid()) {
            args.Process = AttacksEnabled();
        }
    } else {
        args.Process = true;
    }
}

static void OnTeleport(const Events::Teleport::TeleportEventArgs& args) {
    if (Bool(MiscMenu, "autoW") && W.IsReady()) {
        if (args.Type != SDK::TeleportType::Teleport || args.Status != SDK::TeleportStatus::Start) {
            return;
        }
        const auto sender = AIHeroClient(args.Object);
        if (!sender.IsValid() || !sender.IsEnemy()) {
            return;
        }
        if (W.IsInRange(sender)) {
            W.Cast(sender.PreviousPosition());
        }
    }
}

// C#: OnInterruptSpell — địch cast important spell → W hoặc R chặn.
static void OnInterruptSpell(const Events::ProcessSpellEventArgs& args) {
    if (Events::IsLocalPlayer(args.Sender)) {
        return;
    }
    const auto sender = AIHeroClient(args.Sender.Ptr);
    if (!sender.IsValid() || !sender.IsEnemy()) {
        return;
    }
    if (!Extensions::IsCastingInterruptableSpell(sender, false)) {
        return;
    }
    if (Bool(MiscMenu, "InterruptW") && W.IsReady() && ValidHeroTarget(sender, W.Range)) {
        W.Cast(sender.PreviousPosition());
        return;
    } else if (Bool(MiscMenu, "InterruptR") && R.IsReady() && ValidHeroTarget(sender, R.Range) &&
        R.Instance().Name() == "ViktorChaosStorm") {
        R.Cast(sender.PreviousPosition());
    }
}

static void AntiGapCloser(const GapCloserEventArgs& args) {
    if (Bool(AntiGapMenu, "AntiEGap") && W.IsReady()) {
        const auto sender = AIHeroClient(args.Sender);
        if (sender.IsValid() && sender.IsEnemy()) {
            if (ObjectManager::Player().Position().Distance(args.Start) > ObjectManager::Player().Position().Distance(args.End) &&
                ObjectManager::Player().Position().Distance(args.End) <= W.Range) {
                W.Cast(args.End);
            }
        }
    }
}

static void NonKillable(OrbwalkingActionArgs& args) {
    const auto player = Player();
    if ((Orbwalker::ActiveMode() == OrbwalkingMode::LaneClear ||
         Orbwalker::ActiveMode() == OrbwalkingMode::LastHit) &&
        Q.IsReady() && Bool(LaneClearMenu, "LQ") &&
        player.ManaPercent() > static_cast<float>(Slider(LaneClearMenu, "LMana", 40))) {
        const auto target = AIBaseClient(args.Target.Handle());
        if (!target.IsValid()) {
            return;
        }
        if (Q.GetHealthPrediction(target) > 0.0f &&
            Q.GetHealthPrediction(target) < Q.GetDamage(target)) {
            Q.CastOnUnit(target);
        }
    }
}

static void OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }
    if (Bool(DrawMenu, "DrawQ", false) && Q.IsReady()) {
        Drawing::DrawCircle(player.Position(), Q.Range, 0xFFDEB887u);
    }
    if (Bool(DrawMenu, "DrawW", false) && W.IsReady()) {
        Drawing::DrawCircle(player.Position(), W.Range, 0xFFFF0000u);
    }
    if (Bool(DrawMenu, "DrawE", false) && E.IsReady()) {
        Drawing::DrawCircle(player.Position(), 525.0f, 0xFF9400D3u);
    }
    if (Bool(DrawMenu, "DrawMaxE", false) && E.IsReady()) {
        Drawing::DrawCircle(player.Position(), static_cast<float>(maxRangeE), 0xFFFFFF00u);
    }
    if (Bool(DrawMenu, "DrawR", false) && R.IsReady()) {
        Drawing::DrawCircle(player.Position(), R.Range, 0xFFFFA500u);
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

    FollowR();
    AutoKill();
    AutoWCCLogic();

    switch (Orbwalker::ActiveMode()) {
    case OrbwalkingMode::Combo:
        Combo();
        break;
    case OrbwalkingMode::Harass:
        Harass();
        break;
    case OrbwalkingMode::LaneClear:
        Laneclear();
        JungleClear();
        break;
    default:
        break;
    }
}

static void AutoWCCLogic() {
    if (Bool(MiscMenu, "autoW") && W.IsReady()) {
        for (const auto& obj : GameObjects::EnemyHeroes()) {
            if (!ValidHeroTarget(obj, W.Range)) {
                continue;
            }
            const Vector3 ccPos = GetCCBuffPos(obj);
            if (!ccPos.IsZero()) {
                W.Cast(ccPos);
                break;
            }
        }
    }
}

static void FollowR() {
    if (R.Instance().Name() != "ViktorChaosStorm" && Bool(ComboMenu, "CRFlow") &&
        SDK::Variables::TickCount() - lasttick > 0) {
        const auto stormT = TSGetTarget(1100.0f, DamageType::Magical);
        if (stormT.IsValid()) {
            R.Cast(stormT.PreviousPosition());
            lasttick = SDK::Variables::TickCount() + 500;
        }
    }
}

static void AutoKill() {
    for (const auto& obj : GameObjects::EnemyHeroes()) {
        if (!obj.IsEnemy() || !ValidHeroTarget(obj)) {
            continue;
        }
        if (Bool(KillstealMenu, "KQ") && Q.IsReady()) {
            const auto objBase = AIBaseClient(obj.Handle());
            if (Q.IsInRange(obj) && Q.GetDamage(objBase) > obj.Health()) {
                Q.Cast(objBase);
                KillQTargetId = static_cast<uint32_t>(obj.NetworkId());
                KillQTargetValid = true;
                return;
            }
        }
        if (Bool(KillstealMenu, "KE") && E.IsReady() && obj.Health() < GetEDmg(AIBaseClient(obj.Handle()))) {
            PredictCastE(obj);
        }
    }
}

// C#: GetBestLaserFarmLocation — tìm cặp (start,end) laser E trúng nhiều minion nhất.
static NewFarmLocation GetBestLaserFarmLocation(bool jungle) {
    const auto player = Player();
    Vector2 bestendpos{};
    Vector2 beststartpos{};
    int minionCount = 0;
    const int minimalhit = Slider(LaneClearMenu, "LECount", 3);

    std::vector<AIBaseClient> allminions;
    if (!jungle) {
        for (const auto& m : GameObjects::EnemyMinions()) {
            if (ValidTarget(m, static_cast<float>(maxRangeE))) {
                allminions.push_back(AIBaseClient(m.Handle()));
            }
        }
    } else {
        for (const auto& j : GameObjects::Jungle()) {
            if (ValidTarget(j, static_cast<float>(maxRangeE))) {
                allminions.push_back(AIBaseClient(j.Handle()));
            }
        }
    }

    std::vector<Vector2> minionslist;
    for (const auto& m : allminions) {
        minionslist.push_back(m.PreviousPosition().To2D());
    }
    std::vector<Vector2> posiblePositions = minionslist;
    const int max = static_cast<int>(posiblePositions.size());
    for (int i = 0; i < max; ++i) {
        for (int j = 0; j < max; ++j) {
            if (posiblePositions[j] != posiblePositions[i]) {
                posiblePositions.push_back((posiblePositions[j] + posiblePositions[i]) / 2.0f);
            }
        }
    }

    for (const auto& startposminion : allminions) {
        if (player.Distance(startposminion) >= static_cast<float>(rangeE)) {
            continue;
        }
        const Vector2 startPos = startposminion.PreviousPosition().To2D();
        for (const auto& pos : posiblePositions) {
            // C#: pos.Distance(startPos) <= lengthE*lengthE (so sánh dist vs dist^2 — giữ 1-1).
            if (pos.Distance(startPos) <= static_cast<float>(lengthE) * static_cast<float>(lengthE)) {
                const Vector2 endPos = startPos + (pos - startPos).Normalized() * static_cast<float>(lengthE);
                int count = 0;
                for (const auto& pos2 : minionslist) {
                    if (SDK::Collision::detail::DistanceSquaredToSegmentOnly(pos2, startPos, endPos) <= 140.0f * 140.0f) {
                        ++count;
                    }
                }
                if (count >= minionCount) {
                    bestendpos = endPos;
                    minionCount = count;
                    beststartpos = startPos;
                }
            }
        }
    }
    if ((!jungle && minimalhit < minionCount) || (jungle && minionCount > 0)) {
        return NewFarmLocation(beststartpos, bestendpos, minionCount);
    }
    return NewFarmLocation(beststartpos, bestendpos, 0);
}

static bool PredictCastMinionE(bool jungle) {
    const auto farmLoc = GetBestLaserFarmLocation(jungle);
    if (farmLoc.MinionsHit >= Slider(LaneClearMenu, "LECount", 3)) {
        CastE(Vector3::From2D(farmLoc.Position1), Vector3::From2D(farmLoc.Position2));
        return true;
    }
    return false;
}

static void CastE(const Vector3& source, const Vector3& destination) {
    E.Cast(source, destination);
}

static Vector3 BestLaserEnd(const Vector3& start, const AIHeroClient& primary) {
    std::vector<Vector3> predictions;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy, static_cast<float>(maxRangeE))) {
            continue;
        }
        const auto pred = Prediction::GetPrediction(AIBaseClient(enemy.Handle()), E.Delay);
        if (pred.GetCastPosition().IsValid()) {
            predictions.push_back(pred.GetCastPosition());
        }
    }
    if (predictions.empty()) {
        return primary.PreviousPosition();
    }

    Vector3 bestEnd = start.Extend(primary.PreviousPosition(), static_cast<float>(lengthE));
    int bestCount = 0;
    for (const auto& candidate : predictions) {
        if (candidate.Distance(start) < 1.0f) {
            continue;
        }
        const Vector3 end = start.Extend(candidate, static_cast<float>(lengthE));
        int count = 0;
        for (const auto& point : predictions) {
            if (SDK::Collision::detail::DistanceSquaredToSegmentOnly(
                    point.To2D(), start.To2D(), end.To2D()) <= E.Width * E.Width) {
                ++count;
            }
        }
        if (count > bestCount) {
            bestCount = count;
            bestEnd = end;
        }
    }
    return bestEnd;
}

// C#: PredictCastE — laser E 2 điểm, in-range vs out-range logic.
static void PredictCastE(const AIHeroClient& objs) {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }
    const auto target = objs.IsValid() ? objs : TSGetTarget(static_cast<float>(maxRangeE), DamageType::Magical);
    if (!target.IsValid()) {
        return;
    }

    const bool inRange = target.PreviousPosition().DistanceSqr(player.PreviousPosition()) < E.Range * E.Range;
    bool spellCasted = false;
    Vector3 pos1, pos2;

    std::vector<AIHeroClient> nearChamps;
    for (const auto& champ : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(champ, static_cast<float>(maxRangeE)) && champ.NetworkId() != target.NetworkId()) {
            nearChamps.push_back(champ);
        }
    }

    std::vector<AIMinionClient> nearMinions;
    for (const auto& m : GameObjects::EnemyMinions()) {
        if (ValidTarget(m, static_cast<float>(maxRangeE))) {
            nearMinions.push_back(m);
        }
    }

    if (inRange) {
        // C#: E.From dời về phía target 10% lengthE để lấy prediction chuẩn.
        E.From = target.PreviousPosition() +
            (player.PreviousPosition() - target.PreviousPosition()).Normalized() * (static_cast<float>(lengthE) * 0.1f);
        auto prediction = E.GetPrediction(target);
        E.From = player.PreviousPosition();

        if (prediction.GetCastPosition().Distance(player.PreviousPosition()) < E.Range) {
            pos1 = prediction.GetCastPosition();
        } else {
            pos1 = target.PreviousPosition();
        }

        pos2 = BestLaserEnd(pos1, target);
        CastE(pos1, pos2);
        spellCasted = true;

        E.Range = static_cast<float>(rangeE);
        E.From = player.PreviousPosition();
        E.RangeCheckFrom = player.PreviousPosition();
    } else {
        const float startPointRadius = 150.0f;
        const Vector3 startPoint = player.PreviousPosition() +
            (target.PreviousPosition() - player.PreviousPosition()).Normalized() * static_cast<float>(rangeE);

        std::vector<AIHeroClient> targets;
        for (const auto& champ : nearChamps) {
            if (champ.PreviousPosition().DistanceSqr(startPoint) < startPointRadius * startPointRadius &&
                player.PreviousPosition().DistanceSqr(champ.PreviousPosition()) < static_cast<float>(rangeE) * static_cast<float>(rangeE)) {
                targets.push_back(champ);
            }
        }
        if (!targets.empty()) {
            if (targets.size() > 1) {
                std::sort(targets.begin(), targets.end(),
                    [](const AIHeroClient& a, const AIHeroClient& b) { return a.Health() > b.Health(); });
            }
            pos1 = targets[0].PreviousPosition();
        } else {
            std::vector<AIMinionClient> minionTargets;
            for (const auto& minion : nearMinions) {
                if (minion.PreviousPosition().DistanceSqr(startPoint) < startPointRadius * startPointRadius &&
                    player.PreviousPosition().DistanceSqr(minion.PreviousPosition()) < static_cast<float>(rangeE) * static_cast<float>(rangeE)) {
                    minionTargets.push_back(minion);
                }
            }
            if (!minionTargets.empty()) {
                if (minionTargets.size() > 1) {
                    std::sort(minionTargets.begin(), minionTargets.end(),
                        [](const AIMinionClient& a, const AIMinionClient& b) { return a.Health() > b.Health(); });
                }
                pos1 = minionTargets[0].PreviousPosition();
            } else {
                pos1 = startPoint;
            }
        }

        E.From = pos1;
        E.Range = static_cast<float>(lengthE);
        E.RangeCheckFrom = pos1;
        const auto prediction = E.GetPrediction(target);
        if (HitchanceAtLeast(prediction.Hitchance, HitChance::High)) {
            CastE(pos1, BestLaserEnd(pos1, target));
        }

        E.Range = static_cast<float>(rangeE);
        E.From = player.PreviousPosition();
        E.RangeCheckFrom = player.PreviousPosition();
    }
}

static void CastW(const AIHeroClient& unit) {
    const auto target = unit.IsValid() ? unit : TSGetTarget(W.Range, DamageType::Magical);
    if (target.IsValid()) {
        const int wMode = ListIndex(ComboMenu, "CW", 1);
        if (wMode == 0) {
            const auto prds = W.GetPrediction(target);
            if (HitchanceAtLeast(prds.Hitchance, HitChance::Medium)) {
                W.Cast(prds.GetCastPosition());
                return;
            }
        }
        if (wMode == 1) {
            if (target.Path().size() < 2) {
                if (SDK::HasBuffOfType(AIBaseClient(target.Handle()), SDK::BuffType::Slow)) {
                    const auto prds = W.GetPrediction(target);
                    if (HitchanceAtLeast(prds.Hitchance, HitChance::Medium)) {
                        W.Cast(prds.GetCastPosition());
                        return;
                    }
                }
            }
        }
    }
}

static bool CanCastR(const AIHeroClient& unit) {
    const auto player = Player();
    const auto unitBase = AIBaseClient(unit.Handle());
    if (!Bool(ComboMenu, "wasteR")) {
        return true;
    }
    if (!R.IsInRange(unit)) {
        return false;
    }
    if (Q.IsReady() && (Bool(ComboMenu, "CQ") || Bool(KillstealMenu, "KQ"))) {
        if ((Q.IsInRange(unit) && Q.GetDamage(unitBase) > unit.Health()) || IsKillableQTarget(unitBase)) {
            return false;
        }
    }
    if (E.IsReady() && (Bool(ComboMenu, "CE") || Bool(KillstealMenu, "KE"))) {
        if (unit.PreviousPosition().DistanceSqr(player.PreviousPosition()) < static_cast<float>(maxRangeE) * static_cast<float>(maxRangeE) &&
            GetEDmg(unitBase) > unit.Health()) {
            return false;
        }
    }
    if (HealthPred(unitBase, 250) <= 0.0f) {
        return false;
    }
    if (unit.HealthPercent() <= 10.0f && unit.CountAllyHeroesInRange(400.0f) - 1 >= 1) {
        return false;
    }
    return true;
}

static void LogicRKillable() {
    const int rMode = ListIndex(ComboMenu, "CR", 1);
    const int wMode = ListIndex(ComboMenu, "CW", 1);
    for (const auto& obj : TSGetTargets(R.Range, DamageType::Magical)) {
        if (obj.CountEnemyHeroesInRange(300.0f) >= Slider(ComboMenu, "CRCount", 3)) {
            if (wMode == 2 && W.IsReady() && W.IsInRange(obj)) {
                const auto preds = Prediction::GetPrediction(AIBaseClient(obj.Handle()), 0.4f);
                if (preds.GetCastPosition().IsValid()) {
                    W.Cast(preds.GetCastPosition());
                }
            }
            const auto rprds = R.GetPrediction(obj);
            if (HitchanceAtLeast(rprds.Hitchance, HitChance::High)) {
                R.Cast(rprds.GetCastPosition());
            }
        }
        if (UnitIsBlock(obj)) {
            continue;
        }
        if (rMode == 0) {
            if (obj.HealthPercent() <= static_cast<float>(Slider(ComboMenu, "cRheal", 40))) {
                const auto preds = Prediction::GetPrediction(AIBaseClient(obj.Handle()), R.Delay);
                if (preds.GetCastPosition().IsValid()) {
                    R.Cast(preds.GetCastPosition());
                }
            }
            return;
        }
        if (rMode == 1) {
            if (obj.Health() <= GetRDmg(obj) && CanCastR(obj)) {
                const auto preds = Prediction::GetPrediction(AIBaseClient(obj.Handle()), R.Delay);
                if (preds.GetCastPosition().IsValid()) {
                    R.Cast(preds.GetCastPosition());
                }
            }
        }
    }
}

static void Combo() {
    const int wMode = ListIndex(ComboMenu, "CW", 1);
    const int rMode = ListIndex(ComboMenu, "CR", 1);
    if (wMode != 3 && W.IsReady()) {
        CastW(AIHeroClient());
    }
    if (Bool(ComboMenu, "CE") && E.IsReady()) {
        PredictCastE(AIHeroClient());
    }
    if (rMode != 2 && R.IsReady()) {
        LogicRKillable();
    }
    if (Bool(ComboMenu, "CQ") && Q.IsReady()) {
        const auto Qtarget = TSGetTarget(Q.Range + 75.0f, DamageType::Magical);
        if (Qtarget.IsValid() && ValidHeroTarget(Qtarget, Q.Range + Qtarget.BoundingRadius())) {
            const auto qBase = AIBaseClient(Qtarget.Handle());
            Q.CastOnUnit(qBase);
            if (Qtarget.Health() < Q.GetDamage(qBase)) {
                KillQTargetId = static_cast<uint32_t>(Qtarget.NetworkId());
                KillQTargetValid = true;
            }
        }
    }
}

static void Harass() {
    const auto player = Player();
    if (player.ManaPercent() <= static_cast<float>(Slider(HarassMenu, "HMana", 60))) {
        return;
    }
    if (Bool(HarassMenu, "HQ") && Q.IsReady()) {
        const auto qtarget = TSGetTarget(Q.Range, DamageType::Magical);
        if (qtarget.IsValid()) {
            Q.Cast(AIBaseClient(qtarget.Handle()));
        }
    }
    if (Bool(HarassMenu, "HE") && E.IsReady()) {
        PredictCastE(AIHeroClient());
    }
}

static void Laneclear() {
    const auto player = Player();
    if (player.ManaPercent() <= static_cast<float>(Slider(LaneClearMenu, "LMana", 40))) {
        return;
    }
    if (Bool(LaneClearMenu, "LQ") && Q.IsReady()) {
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (!ValidTarget(minion, Q.Range)) {
                continue;
            }
            const auto mBase = AIBaseClient(minion.Handle());
            // GetHealthPrediction bất biến trong 1 vòng cho cùng unit — tính 1 lần,
            // dùng lại cho cả 2 so sánh (>0 và <damage) = kết quả y hệt.
            const float hpPred = Q.GetHealthPrediction(mBase);
            if (hpPred > 0.0f &&
                hpPred < Q.GetDamage(mBase) &&
                (!player.Spellbook().IsWindingUp() || Orbwalker::CanAttack())) {
                Q.CastOnUnit(mBase);
                break;
            }
        }
    }
    if (Bool(LaneClearMenu, "LE") && E.IsReady()) {
        PredictCastMinionE(false);
    }
}

static void JungleClear() {
    const auto player = Player();
    if (player.ManaPercent() <= static_cast<float>(Slider(LaneClearMenu, "LMana", 40))) {
        return;
    }
    if (Bool(JungleClearMenu, "JQ") && Q.IsReady()) {
        AIMinionClient junglsFirst;
        for (const auto& j : GameObjects::Jungle()) {
            if (ValidTarget(j, Q.Range)) {
                junglsFirst = j;
                break;
            }
        }
        if (junglsFirst.IsValid()) {
            Q.Cast(AIBaseClient(junglsFirst.Handle()));
        }
    }
    if (Bool(JungleClearMenu, "JE") && E.IsReady()) {
        PredictCastMinionE(true);
    }
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnDoCast -= &OnDoCastSpell;
    Events::hook.OnProcessSpell -= &OnProcessSpell;
    Events::hook.OnProcessSpell -= &OnInterruptSpell;
    Orbwalker::OnBeforeAttack -= &OnBeforeAttack;
    Events::hook.OnTeleport -= &OnTeleport;
    Events::hook.OnGapCloser -= &AntiGapCloser;
    Orbwalker::OnNonKillableMinion -= &NonKillable;
    Drawing::OnDraw -= &OnDraw;

    Loaded = false;
}

} // namespace Plugins::SharpAIO::Viktor
