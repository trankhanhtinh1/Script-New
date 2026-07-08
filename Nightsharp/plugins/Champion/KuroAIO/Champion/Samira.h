#pragma once

#include "../Helper/KuroAIOCommon.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include <vector>

namespace Plugins::KuroAIO::Samira {

using SDK::Core::Utils::AutoAttack;

inline Menu* MenuRoot = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* MiscMenu = nullptr;
inline Menu* KeysMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 900.0f };
inline Spell W{ SpellSlot::W, 325.0f };
inline Spell E{ SpellSlot::E, 600.0f };
inline Spell R{ SpellSlot::R, 600.0f };

inline bool Loaded = false;
inline int LastE = 0;
inline int LastW = 0;
inline int LastCastSpell = 0;
inline int LastBeforeAttack = 0;
inline int LastOnAttack = 0;
inline int LastAfterAttack = 0;

static float ManaPercent() {
    const auto player = Player();
    return player.IsValid() ? player.ManaPercent() : 0.0f;
}

static int CountEnemyHeroesNear(const Vector3& position, float range) {
    int count = 0;
    const float rangeSqr = range * range;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (!ValidHeroTarget(enemy)) {
            continue;
        }
        if (enemy.Position().DistanceSqr2D(position) <= rangeSqr) {
            ++count;
        }
    }
    return count;
}

static Vector3 DashPositionTo(const Vector3& targetPosition) {
    const auto player = Player();
    if (!player.IsValid()) {
        return {};
    }

    Vector3 position = player.Position().Extend(targetPosition, E.Range);
    position.y = NavMesh::GetHeightForPosition(position);
    return position;
}

static bool UnderTower(const Vector3& position) {
    const auto player = Player();
    const float extraRadius = player.IsValid() ? player.BoundingRadius() : 65.0f;
    const float range = 850.0f + extraRadius;
    const float rangeSqr = range * range;

    for (const auto& turret : GameObjects::EnemyTurrets()) {
        if (!turret.IsValid() || turret.IsDead()) {
            continue;
        }
        if (turret.Position().DistanceSqr2D(position) <= rangeSqr) {
            return true;
        }
    }

    for (const auto& spawn : GameObjects::EnemySpawnPoints()) {
        if (spawn.IsValid() && spawn.Position().DistanceSqr2D(position) <= rangeSqr) {
            return true;
        }
    }
    return false;
}

static bool AllowDashTo(const Vector3& position) {
    if (position.IsZero() || NavMesh::IsWall(position)) {
        return false;
    }
    return Key(KeysMenu, "AllowTurret") || !UnderTower(position);
}

static bool AllowDashTo(const AIBaseClient& target) {
    return target.IsValid() && AllowDashTo(DashPositionTo(target.Position()));
}

static bool CastPosition(Spell& spell, const Vector3& position) {
    return spell.IsReady() && spell.Cast(position);
}

static bool CastUnit(Spell& spell, const AIBaseClient& target) {
    return spell.IsReady() &&
           ValidTarget(target, spell.CurrentRange()) &&
           spell.Cast(target) == CastStates::SuccessfullyCasted;
}

static bool CastQOnTarget(const AIHeroClient& target,
                          bool allowGun,
                          bool allowBlade,
                          HitChance hitChance = HitChance::High) {
    if (!Q.IsReady() || !ValidHeroTarget(target, Q.Range)) {
        return false;
    }

    const auto player = Player();
    if (!player.IsValid()) {
        return false;
    }

    const auto pred = Q.GetPrediction(target);
    const Vector3 castPosition = pred.GetCastPosition();
    const float closeRange = Q.Range / 3.0f;
    const bool close = player.Distance(castPosition) <= closeRange;

    if (close && allowBlade) {
        bool res = Q.Cast(target) == CastStates::SuccessfullyCasted;
        const bool targetCastSuccess = res;
        if (!res) {
            res = CastPosition(Q, castPosition);
        }
        return res;
    }

    if (!close && allowGun &&
        static_cast<int>(pred.Hitchance) >= static_cast<int>(hitChance) &&
        pred.CollisionObjects.empty()) {
        bool res = Q.Cast(target) == CastStates::SuccessfullyCasted;
        const bool targetCastSuccess = res;
        if (!res) {
            res = CastPosition(Q, castPosition);
        }

        return res;
    }

    return false;
}

static bool CastQClear(const AIBaseClient& target) {
    if (!Q.IsReady() || !ValidTarget(target, Q.Range)) {
        return false;
    }
    return Q.Cast(target) == CastStates::SuccessfullyCasted;
}

static float GetEDmg(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }

    static constexpr float eBase[] = { 0.0f, 50.0f, 60.0f, 70.0f, 80.0f, 90.0f };
    static constexpr float rBase[] = { 0.0f, 10.0f, 20.0f, 30.0f };
    const int eLevel = std::clamp(E.Level(), 0, 5);
    const int rLevel = std::clamp(R.Level(), 0, 3);

    float damage = player.CalculateMagicDamage(target, eBase[eLevel] + 0.2f * player.BonusAttackDamage());
    damage += Damage::GetAutoAttackDamage(player, target);
    if (player.HasBuff("SamiraR") || R.IsReady()) {
        damage += player.CalculatePhysicalDamage(target, (rBase[rLevel] + 0.6f * player.TotalAttackDamage()) * 2.0f);
    }
    return damage;
}

static bool CastE(const AIBaseClient& target) {
    return CastUnit(E, target);
}

static bool TryEKillSteal() {
    if (!E.IsReady() || !Bool(EMenu, "EKs")) {
        return false;
    }

    for (const auto& target : EnemyHeroesByHealth(E.Range)) {
        if (target.Health() <= GetEDmg(target) && AllowDashTo(target)) {
            if (CastE(target)) {
                return true;
            }
        }
    }
    return false;
}

static void ApplyDefaultSettings() {
    SetBool(QMenu, "QGunCombo", true);
    SetBool(QMenu, "QBladeCombo", true);
    SetSlider(QMenu, "QManaCheck", 30);

    SetBool(WMenu, "WBlock", true);
    SetBool(WMenu, "WonlyBlockIncombo", false);
    SetBool(WMenu, "WCantAA", false);
    SetSlider(WMenu, "EnemyCount", 1);

    SetBool(EMenu, "ECombo", true);
    SetBool(EMenu, "EQ", false);
    SetBool(EMenu, "EW", true);
    SetKey(EMenu, "EMinions", false);
    SetSlider(EMenu, "EHealth", 70);
    SetBool(EMenu, "ER", true);
    SetBool(EMenu, "EKs", true);

    SetBool(RMenu, "RCombo", true);
    SetSlider(RMenu, "RCount", 1);
    SetBool(RMenu, "AutoE", true);

    SetBool(MiscMenu, "WaitForAA", true);
    SetSlider(MiscMenu, "AATimer", 1500);
    SetBool(MiscMenu, "PacketCast", false);
    SetBool(MiscMenu, "DrawQAARange", false);
}

static void BuildMenu() {
    MenuRoot = new Menu("champion.kuroaio.samira", "Kuro - Samira", true);

    QMenu = MenuRoot->AddSubMenu(new Menu("Q Samira Settings", "Q Settings"));
    QMenu->Add(new MenuBool("QGunCombo", "Q Gun Combo"));
    QMenu->Add(new MenuBool("QBladeCombo", "Q Blade Combo"));
    QMenu->Add(new MenuSeparator("QHarass", "Q Harass | Clear"));
    QMenu->Add(new MenuSlider("QManaCheck", "If Mana > ", 30, 0, 100));

    WMenu = MenuRoot->AddSubMenu(new Menu("W Samira Settings", "W Settings"));
    WMenu->Add(new MenuBool("WBlock", "W Block Attack"));
    WMenu->Add(new MenuBool("WonlyBlockIncombo", "Block AA in Combo only", false));
    WMenu->Add(new MenuBool("WCantAA", "W if cant AA", false));
    WMenu->Add(new MenuSlider("EnemyCount", "Targets Count >= ", 1, 1, 5));

    EMenu = MenuRoot->AddSubMenu(new Menu("E Samira Settings", "E Settings"));
    EMenu->Add(new MenuBool("ECombo", "E Combo"));
    EMenu->Add(new MenuBool("EQ", "Cast Q when E", false));
    EMenu->Add(new MenuBool("EW", "EW Combo"));
    EMenu->Add(new MenuKeyBind("EMinions", "Accept E on Minion", SDK::Keys::A, KeyBindType::Toggle));
    EMenu->Add(new MenuSeparator("Eonly", "But Only When"));
    EMenu->Add(new MenuSlider("EHealth", "Target Health % <= ", 50, 0, 100));
    EMenu->Add(new MenuBool("ER", "R Is In Ready"));
    EMenu->Add(new MenuBool("EKs", "E Ks"));

    RMenu = MenuRoot->AddSubMenu(new Menu("R Samira Settings", "R Settings"));
    RMenu->Add(new MenuBool("RCombo", "R Combo"));
    RMenu->Add(new MenuSlider("RCount", "Target Hit >= ", 1, 1, 5));
    RMenu->Add(new MenuBool("AutoE", "Auto E When Using R"));

    MiscMenu = MenuRoot->AddSubMenu(new Menu("Misc Samira Settings", "Misc Settings"));
    MiscMenu->Add(new MenuBool("WaitForAA", "Waiting For AA"));
    MiscMenu->Add(new MenuSlider("AATimer", "Max AA wait (ms)", 1500, 0, 2500));
    MiscMenu->Add(new MenuBool("PacketCast", "Packet Cast", false));
    MiscMenu->Add(new MenuBool("DrawQAARange", "Draw AA & Q Range", false));

    KeysMenu = MenuRoot->AddSubMenu(new Menu("Keys Samira Settings", "Keys Settings"));
    KeysMenu->Add(new MenuKeyBind("QMixed", "Q Harass When Clear", SDK::Keys::H, KeyBindType::Toggle, true));
    KeysMenu->Add(new MenuKeyBind("QClear", "Q Clear Minions", SDK::Keys::H, KeyBindType::Toggle, true));
    KeysMenu->Add(new MenuKeyBind("AllowTurret", "Allow Turret Key [Toggle]", SDK::Keys::T, KeyBindType::Toggle));
    KeysMenu->Add(new MenuKeyBind("TurboFast", "Turbo Fastly", SDK::Keys::Z, KeyBindType::Toggle));

    MenuRoot->Add(new MenuBool("reset", "Reset Samira", false));
    MenuRoot->Attach();
}

static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void OnBeforeAttack(OrbwalkingActionArgs& args);
static void OnAttack(OrbwalkingActionArgs& args);
static void OnAfterAttack(OrbwalkingActionArgs& args);
static void OnProcessSpell(const Events::ProcessSpellEventArgs& args);
static void OnDraw();
static void Check();
static void Combo();
static void HarassAndClear();
static void JungleClear();
static void RemoveMenu();
static void OnUnload();

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q, 900.0f);
    Q.SetSkillshot(0.25f, 50.0f, 2600.0f, true, SkillshotType::SkillshotLine);
    W = Spell(SpellSlot::W, 325.0f);
    E = Spell(SpellSlot::E, 600.0f);
    E.SetTargetted(0.0f, 2000.0f);
    R = Spell(SpellSlot::R, 600.0f);

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Events::hook.OnProcessSpell += &OnProcessSpell;
    Orbwalker::OnBeforeAttack += &OnBeforeAttack;
    Orbwalker::OnAttack += &OnAttack;
    Orbwalker::OnAfterAttack += &OnAfterAttack;
    Drawing::OnDraw += &OnDraw;

    Loaded = true;
    Game::Print("<font color='#b756c5' size='20'>Kuro - Samira loaded</font>");
}

static void OnBeforeAttack(OrbwalkingActionArgs&) {
    LastBeforeAttack = SDK::Variables::TickCount();
}

static void OnAttack(OrbwalkingActionArgs&) {
    LastOnAttack = SDK::Variables::TickCount();
}

static void OnAfterAttack(OrbwalkingActionArgs&) {
    LastAfterAttack = SDK::Variables::TickCount();
    LastCastSpell = 0;
}

static bool InAttackAction() {
    return Orbwalker::IsAutoAttacking();
}

static bool AfterAttackRecently() {
    return Recent(LastAfterAttack, 450);
}

static bool ShouldWaitForAutoAttack() {

    if (!Bool(MiscMenu, "WaitForAA", true)) {
        return false;
    }

    if (Orbwalker::IsWindingUp()) {
        return true;
    }

    const int time = Slider(MiscMenu, "AATimer", 1500);

    if (LastCastSpell + time >= SDK::Variables::TickCount()) {
        return true;
    }

    return false;
}

static void Check() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }

    if (auto* reset = MenuRoot ? MenuRoot->Get<MenuBool>("reset") : nullptr) {
        if (reset->Value) {
            ApplyDefaultSettings();
            reset->Value = false;
        }
    }

    if (player.HasBuff("SamiraR") || (player.HasBuff("SamiraW") && LastE < LastW)) {
        Orbwalker::Move(Game::CursorPos());
    }

    (void)TryEKillSteal();
}

static bool TryTurboCombo() {
    if (!Key(KeysMenu, "TurboFast") || !Q.IsReady() || (!W.IsReady() && !R.IsReady() && !Player().HasBuff("SamiraR")) || !E.IsReady()) {
        return false;
    }

    const auto target = GetPhysicalTarget(E.Range);
    if (!ValidHeroTarget(target, E.Range)) {
        return false;
    }

    // WEQ Combo if enemy in W range
    bool enemyInW = false;
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        if (ValidHeroTarget(enemy, W.Range)) {
            enemyInW = true;
            break;
        }
    }

    if (LastCastSpell == 0 || enemyInW || Orbwalker::AttackCooldownRemaining() > 500) {
        if (AllowDashTo(target)) {

            if (R.IsReady() && Bool(RMenu, "RCombo")){
                R.Cast();

                if (CastE(target)) {
                    return Q.Cast(target.Position());;
                }

                return false;
            }

            if (W.IsReady()) {
                return W.Cast();
            }

            if (CastE(target)) {
                return Q.Cast(target.Position());;
            }
        }
    }

    return false;
}

static bool TryRCombo() {
    const auto player = Player();
    if (!player.IsValid() || !R.IsReady() || !Bool(RMenu, "RCombo")) {
        return false;
    }

    if (player.CountEnemyHeroesInRange(R.Range) < Slider(RMenu, "RCount", 1)) {
        return false;
    }

    if (Bool(RMenu, "AutoE") && E.IsReady()) {
        for (const auto& target : EnemyHeroesByHealth(E.Range)) {
            const Vector3 dashPos = DashPositionTo(target.Position());
            if (AllowDashTo(dashPos) &&
                CountEnemyHeroesNear(dashPos, R.Range) > Slider(RMenu, "RCount", 1)) {
                if (CastE(target) && R.Cast()) {
                    return true;
                }
            }
        }
    }

    const auto target = GetPhysicalTarget(R.Range);
    if (ValidHeroTarget(target, R.Range)) {
        return R.Cast();
    }
    return false;
}

static bool TryEMinionGapClose() {
    if (!E.IsReady() || !Bool(EMenu, "ECombo") || !Key(EMenu, "EMinions")) {
        return false;
    }

    const auto target = GetPhysicalTarget(E.Range + Q.Range);
    if (!ValidHeroTarget(target, E.Range + Q.Range)) {
        return false;
    }

    const auto player = Player();
    if (!player.IsValid() || target.HealthPercent() > static_cast<float>(Slider(EMenu, "EHealth", 50))) {
        return false;
    }

    if (Bool(EMenu, "ER") && !R.IsReady()) {
        return false;
    }

    AIMinionClient best;
    float bestDistance = FLT_MAX;
    for (const auto& minion : GameObjects::EnemyMinions()) {
        if (!ValidTarget(minion, E.Range)) {
            continue;
        }

        const Vector3 dashPos = DashPositionTo(minion.Position());
        if (!AllowDashTo(dashPos)) {
            continue;
        }

        const float distanceAfterDash = dashPos.Distance(target.Position());
        if (distanceAfterDash < player.Distance(target) && distanceAfterDash < bestDistance) {
            best = minion;
            bestDistance = distanceAfterDash;
        }
    }

    return best.IsValid() && CastE(best);
}
static bool TryECombo() {
    const auto player = Player();
    if (!player.IsValid() || !E.IsReady() || !Bool(EMenu, "ECombo")) {
        return false;
    }

    if (TryEMinionGapClose()) {
        return true;
    }

    for (const auto& target : EnemyHeroesByHealth(E.Range)) {
        if (!AllowDashTo(target)) {
            continue;
        }

        const auto pred = Q.GetPrediction(target);
        if (player.Distance(pred.GetUnitPosition()) > E.Range && CastE(target)) {
            return true;
        }

        if (target.Health() < GetEDmg(target) && Bool(EMenu, "EKs") && CastE(target)) {
            return true;
        }

        if (player.HasBuff("SamiraR") && Bool(RMenu, "AutoE")) {
            const Vector3 dashPos = DashPositionTo(target.Position());
            if (CountEnemyHeroesNear(dashPos, R.Range) > player.CountEnemyHeroesInRange(R.Range) &&
                CastE(target)) {

                return true;
            }
        }

        if (player.HasBuff("SamiraW") && Bool(EMenu, "EW") &&
            SDK::Variables::TickCount() > LastW + 750 &&
            CastE(target)) {
            return true;
        }

        if (Q.IsReady() && Bool(EMenu, "EQ") && CastE(target)) {
            Q.Cast(target.Position());
            return true;
        }
    }
    return false;
}

static bool TryWCombo() {
    if (ShouldWaitForAutoAttack()) {
        return false;
    }

    if (W.IsReady() && Bool(WMenu, "WCantAA") && !Orbwalker::CanAttack()) {
        if (!EnemyHeroesByHealth(E.IsReady() ? E.Range : W.Range).empty()) {
            return W.Cast();
        }
    }

    if (W.IsReady() && E.IsReady() && Bool(EMenu, "EW")) {
        for (const auto& target : EnemyHeroesByHealth(E.Range)) {
            if (target.HealthPercent() < 60.0f && AllowDashTo(target)) {
                return W.Cast();
            }
        }
    }
    return false;
}

static bool TryQCombo() {

    if (!Q.IsReady() || ShouldWaitForAutoAttack()) {
        return false;
    }

    const auto target = GetPhysicalTarget(Q.Range);
    if (!ValidHeroTarget(target, Q.Range)) {
        return false;
    }

    if (Player().IsDashing() || LastE + 700 > SDK::Variables::TickCount()) {   
        return Bool(EMenu, "EQ") && Q.Cast(target.Position());
    }

    return CastQOnTarget(
        target,
        Bool(QMenu, "QGunCombo"),
        Bool(QMenu, "QBladeCombo"));
}

static void Combo() {
    const auto player = Player();

    if (!player.IsValid() || player.IsDead() || InAttackAction() || !IsComboMode()) {
        return;
    }

    if (TryTurboCombo()) {
        return;
    }
    if (TryRCombo()) {
        return;
    }
    if (TryECombo()) {
        return;
    }
    if (TryWCombo()) {
        return;
    }
    (void)TryQCombo();
}

static void HarassAndClear() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead()) {
        return;
    }

    if (ManaPercent() <= static_cast<float>(Slider(QMenu, "QManaCheck", 30))) {
        return;
    }

    if (IsHarassMode() && Key(KeysMenu, "QMixed")) {
        const auto target = GetPhysicalTarget(Q.Range);
        if (ValidHeroTarget(target, Q.Range)) {
            (void)CastQOnTarget(target, true, true);
        }
        return;
    }

    if (IsClearMode() && Key(KeysMenu, "QClear")) {
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (ValidTarget(minion, Q.Range) && CastQClear(minion)) {
                return;
            }
        }
    } else if (IsLastHitMode()) {
        for (const auto& minion : GameObjects::EnemyMinions()) {
            if (ValidTarget(minion, Q.Range) && minion.Health() < Q.GetDamage(minion)) {
                if (CastQClear(minion)) {
                    return;
                }
            }
        }
    }
}

static void JungleClear() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || !IsClearMode() || !Q.IsReady()) {
        return;
    }

    if (ManaPercent() <= static_cast<float>(Slider(QMenu, "QManaCheck", 30))) {
        return;
    }

    std::vector<AIMinionClient> jungle;
    for (const auto& mob : GameObjects::Jungle()) {
        if (ValidTarget(mob, Q.Range)) {
            jungle.push_back(mob);
        }
    }
    std::sort(jungle.begin(), jungle.end(), [](const AIMinionClient& a, const AIMinionClient& b) {
        return a.MaxHealth() > b.MaxHealth();
    });

    if (!jungle.empty()) {
        (void)CastQClear(jungle.front());
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    Check();
    Combo();
    HarassAndClear();
    JungleClear();
}

static void OnProcessSpell(const Events::ProcessSpellEventArgs& args) {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    const int slot = args.Slot;
    const int now = SDK::Variables::TickCount();
    if (Events::IsLocalPlayer(args.Sender)) {
        if (slot == static_cast<int>(SpellSlot::E)) {
            LastCastSpell = now;
            LastE = now;
            Orbwalker::ResetAutoAttackTimer();
        }
        if (slot == static_cast<int>(SpellSlot::W)) {
            LastCastSpell = now;
            LastW = now;
        }
        if ((args.IsAutoAttack || AutoAttack::IsAutoAttack(args.SpellName)) &&
            args.Target.IsValid() &&
            args.Target.Type == ::Core::Objects::ObjectType::AIHeroClient) {
            LastOnAttack = now;
            LastCastSpell = 0;
        }else{
            LastCastSpell = now;
        }
        return;
    }

    if (!args.Sender.IsValid() ||
        args.Sender.Team == static_cast<std::uint32_t>(player.Team()) ||
        args.Sender.Type != ::Core::Objects::ObjectType::AIHeroClient ||
        slot < static_cast<int>(SpellSlot::Q) ||
        slot > static_cast<int>(SpellSlot::R)) {
        return;
    }

    const bool targetsPlayer =
        (args.Target.IsValid() && args.Target.NetworkId == static_cast<std::uint32_t>(player.NetworkId())) ||
        (args.TargetNetworkId != 0 &&
         args.TargetNetworkId != 0xFFFFFFFFu &&
         args.TargetNetworkId == static_cast<std::uint32_t>(player.NetworkId()));

    if (!targetsPlayer ||
        !W.IsReady() ||
        !Bool(WMenu, "WBlock") ||
        (Bool(WMenu, "WonlyBlockIncombo", false) && !IsComboMode()) ||
        InAttackAction()) {
        return;
    }

    if (player.CountEnemyHeroesInRange(W.Range + E.Range) >= Slider(WMenu, "EnemyCount", 1)) {
        (void)W.Cast();
    }
}

static void OnDraw() {
    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || !Bool(MiscMenu, "DrawQAARange", false)) {
        return;
    }

    Drawing::DrawCircle(
        player.Position(),
        AutoAttack::GetRealAutoAttackRange(player),
        0xFFFF0000u,
        1.5f,
        64);
    Drawing::DrawCircle(
        player.Position(),
        Q.Range,
        0xFFFFFF00u,
        1.5f,
        64);
}

static void RemoveMenu() {
    if (MenuRoot) {
        MenuManager::Instance().Remove(MenuRoot);
    }
    MenuRoot = nullptr;
    QMenu = nullptr;
    WMenu = nullptr;
    EMenu = nullptr;
    RMenu = nullptr;
    MiscMenu = nullptr;
    KeysMenu = nullptr;
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Events::hook.OnProcessSpell -= &OnProcessSpell;
    Orbwalker::OnBeforeAttack -= &OnBeforeAttack;
    Orbwalker::OnAttack -= &OnAttack;
    Orbwalker::OnAfterAttack -= &OnAfterAttack;
    Drawing::OnDraw -= &OnDraw;
    RemoveMenu();

    Loaded = false;
}

} // namespace Plugins::KuroAIO::Samira
