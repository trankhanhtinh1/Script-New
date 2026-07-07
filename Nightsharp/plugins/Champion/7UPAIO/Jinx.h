#pragma once

#include "Common.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace Plugins::AIO7UP::Jinx {

using SDK::Core::Utils::AutoAttack;
using namespace Common;

inline Menu* MenuRoot = nullptr;
inline Menu* QMenu = nullptr;
inline Menu* WMenu = nullptr;
inline Menu* EMenu = nullptr;
inline Menu* RMenu = nullptr;
inline Menu* DrawMenu = nullptr;

inline Spell Q{ SpellSlot::Q, 685.0f };
inline Spell W{ SpellSlot::W, 1500.0f };
inline Spell E{ SpellSlot::E, 925.0f };
inline Spell R{ SpellSlot::R, 3000.0f };

inline bool Loaded = false;
inline bool FishBoneActive = false;
inline bool ComboMode = false;
inline bool FarmMode = false;
inline float BigGunRange = 1200.0f;
inline float WCastTime = 0.0f;
inline DWORD LastUpdateTick = 0;
inline DWORD LastQTick = 0;
inline DWORD LastWTick = 0;
inline DWORD LastETick = 0;
inline DWORD LastRTick = 0;

static constexpr const char* kDangerSpells[] = {
    "katarinar",
    "drain",
    "consume",
    "absolutezero",
    "staticfield",
    "reapthewhirlwind",
    "jinxw",
    "jinxr",
    "shenstandunited",
    "threshe",
    "threshrpenta",
    "threshq",
    "meditate",
    "caitlynpiltoverpeacemaker",
    "volibearqattack",
    "cassiopeiapetrifyinggaze",
    "ezrealtrueshotbarrage",
    "galioidolofdurand",
    "luxmalicecannon",
    "missfortunebullettime",
    "infiniteduress",
    "alzaharnethergrasp",
    "lucianq",
    "velkozr",
    "rocketgrabmissile",
};

static void BuildMenu();
static void Game_OnUpdate(const GameUpdateEventArgs& args);
static void Orbwalker_OnAfterAttack(OrbwalkingActionArgs& args);
static void OnDoCast(const Events::ProcessSpellEventArgs& args);
static void OnGapcloser(const GapCloserEventArgs& args);
static void OnTeleport(const Events::Teleport::TeleportEventArgs& args);
static void OnUnload();

static bool EqualsIgnoreCase(const char* left, const char* right) {
    return left && right && _stricmp(left, right) == 0;
}

static std::string MenuKey(const char* prefix, const AIHeroClient& hero) {
    return std::string(prefix) + hero.CharacterName();
}

static bool HasHardCc(const AIBaseClient& target) {
    if (!target.IsValid()) {
        return false;
    }

    const uintptr_t address = target.Address();
    return CoreBuffs::HasBuffType(address, 5) ||
           CoreBuffs::HasBuffType(address, 8) ||
           CoreBuffs::HasBuffType(address, 9) ||
           CoreBuffs::HasBuffType(address, 11) ||
           CoreBuffs::HasBuffType(address, 12) ||
           CoreBuffs::HasBuffType(address, 22) ||
           CoreBuffs::HasBuffType(address, 24) ||
           CoreBuffs::HasBuffType(address, 25) ||
           CoreBuffs::HasBuffType(address, 29) ||
           CoreBuffs::HasBuffType(address, 30) ||
           CoreBuffs::HasBuffType(address, 31) ||
           CoreBuffs::HasBuffType(address, 35);
}

static bool DangerousSpellName(const char* spellName) {
    if (!spellName || !spellName[0]) {
        return false;
    }

    char lowered[128] = {};
    strncpy_s(lowered, spellName, _TRUNCATE);
    for (char& c : lowered) {
        if (!c) {
            break;
        }
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    for (const char* known : kDangerSpells) {
        if (EqualsIgnoreCase(lowered, known)) {
            return true;
        }
    }
    return false;
}

static bool CastSkillshot(Spell& spell,
                          const AIBaseClient& target,
                          HitChance hitChance,
                          bool checkCollision = true) {
    if (!spell.IsReady() || !ValidTarget(target, spell.CurrentRange())) {
        return false;
    }

    const auto pred = spell.GetPrediction(target);
    if (checkCollision && !pred.CollisionObjects.empty()) {
        return false;
    }

    if (!HitchanceAtLeast(pred.Hitchance, hitChance)) {
        return false;
    }

    return CastPosition(spell, pred.GetCastPosition(), target);
}

static float RealPowPowRange(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid()) {
        return 0.0f;
    }

    return 650.0f + player.BoundingRadius() + (target.IsValid() ? target.BoundingRadius() : 0.0f);
}

static float RealDistance(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return FLT_MAX;
    }

    return player.Position().Distance2D(target.Position()) +
           player.BoundingRadius() +
           target.BoundingRadius();
}

static int CountLaneMinionsInRange(float range, const Vector3& position) {
    int count = 0;
    const float rangeSqr = range * range;
    for (const auto& minion : GameObjects::EnemyLaneMinions()) {
        if (ValidTarget(minion) && minion.Position().DistanceSqr2D(position) <= rangeSqr) {
            ++count;
        }
    }
    return count;
}

static float HealthRegenRate(const AIBaseClient& target) {
    return target.IsValid() ? ::CoreAIHeroClient::HealthRegenRate(target.Address()) : 0.0f;
}

static float PercentLifeSteal(const AIBaseClient& target) {
    return target.IsValid() ? ::CoreAIHeroClient::PercentLifeSteal(target.Address()) : 0.0f;
}

static float FlatPhysicalDamageMod(const AIBaseClient& target) {
    return target.IsValid() ? ::CoreAIHeroClient::FlatPhysicalDamageMod(target.Address()) : 0.0f;
}

static float KsDamage(const AIBaseClient& target, Spell& spell) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }

    float damage = spell.GetDamage(target);
    if (player.HasBuff("summonerexhaust")) {
        damage *= 0.6f;
    }
    if (target.HasBuff("ferocioushowl")) {
        damage *= 0.7f;
    }

    if (target.IsHero()) {
        const AIHeroClient hero(target.Handle());
        if (hero.CharacterName() == "Blitzcrank" &&
            !hero.HasBuff("BlitzcrankManaBarrierCD") &&
            !hero.HasBuff("ManaBarrier")) {
            damage -= hero.Mana() * 0.5f;
        }
    }

    const float predicted = SDK::HealthPrediction::GetPrediction(target, 500);
    damage += target.Health() - predicted;
    damage -= HealthRegenRate(target);
    damage -= PercentLifeSteal(target) * 0.005f * FlatPhysicalDamageMod(target);
    return std::max(0.0f, damage);
}

static bool WValidRange(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid() || !W.IsReady()) {
        return false;
    }

    const float range = RealDistance(target);
    const int mode = ListIndex(WMenu, "Wmode", 0);
    if (mode == 0) {
        const float powPowRange = RealPowPowRange(target);
        return range > powPowRange && player.CountEnemyHeroesInRange(powPowRange) == 0;
    }
    if (mode == 1) {
        return range > Q.Range + 50.0f && player.CountEnemyHeroesInRange(Q.Range + 50.0f) == 0;
    }

    const float customRange = static_cast<float>(Slider(WMenu, "Wcustome", 600));
    return range > customRange && player.CountEnemyHeroesInRange(customRange) == 0;
}

static void FishBoneToMiniGun(const AIBaseClient& target) {
    const auto player = Player();
    if (!player.IsValid() || !target.IsValid() || !Q.IsReady()) {
        return;
    }

    // NOTE: an earlier `IsWindingUp()` guard here (meant to avoid a lost attack
    // beat when switching) blocked the switch ENTIRELY when the target was in AA
    // range — Jinx is perpetually mid-attack there, so it never switched to
    // MiniGun. Switcheroo is an instant toggle that does not cancel an in-flight
    // auto-attack, so no windup guard is needed: switch as soon as the target is
    // inside MiniGun (pow-pow) range.
    if (RealDistance(target) >= RealPowPowRange(target)) {
        return;
    }

    if (target.CountEnemyHeroesInRange(250.0f) >= Slider(QMenu, "Qaoe", 3)) {
        return;
    }

    const float aaDamage = SDK::Damage::GetAutoAttackDamage(player, target);
    if (player.ManaPercent() < static_cast<float>(Slider(QMenu, "QmanaCombo", 10)) ||
        aaDamage * static_cast<float>(Slider(QMenu, "QmanaIgnore", 3)) < target.Health()) {
        Q.Cast();
    }
}

static void UpdateState() {
    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    FishBoneActive = player.HasBuff("JinxQ");
    ComboMode = Orbwalker::ActiveMode() == OrbwalkingMode::Combo;
    FarmMode =
        Orbwalker::ActiveMode() == OrbwalkingMode::LaneClear ||
        Orbwalker::ActiveMode() == OrbwalkingMode::LastHit ||
        Orbwalker::ActiveMode() == OrbwalkingMode::Harass;

    Q.Range = 685.0f + player.BoundingRadius() + 25.0f * static_cast<float>(Q.Level());
    BigGunRange = std::max(Q.Range + 500.0f, 1200.0f);
}

static bool IsLaneMinionTarget(const AttackableUnit& unit) {
    if (!unit.IsValid()) {
        return false;
    }

    const AIMinionClient minion(unit.Handle());
    return minion.IsValid() && minion.IsMinion() && !minion.IsJungle() &&
           !minion.IsPlant() && !minion.IsPet() && !minion.IsClone();
}

static void QLogic() {
    const auto player = Player();
    if (!player.IsValid() || !Q.IsReady()) {
        return;
    }

    const auto orbTarget = Orbwalker::GetTarget();
    if (FishBoneActive) {
        if (Orbwalker::ActiveMode() == OrbwalkingMode::LaneClear &&
            player.ManaPercent() > static_cast<float>(Slider(QMenu, "QmanaLC", 80)) &&
            IsLaneMinionTarget(orbTarget)) {
            return;
        }

        const AIBaseClient orbBase(orbTarget.Handle());
        if (ListIndex(QMenu, "Qchange", 1) == 0 && orbBase.IsHero()) {
            FishBoneToMiniGun(orbBase);
            return;
        }

        if (!ComboMode && Orbwalker::ActiveMode() != OrbwalkingMode::None) {
            Q.Cast();
        }
        return;
    }

    const auto target = GetTarget(Q.Range + 40.0f, DamageType::Physical);
    if (ValidHeroTarget(target, Q.Range + 40.0f)) {
        const float aaDamage = SDK::Damage::GetAutoAttackDamage(player, target);
        const bool needsRocket =
            !AutoAttack::InAutoAttackRange(target) ||
            target.CountEnemyHeroesInRange(250.0f) >= Slider(QMenu, "Qaoe", 3);
        if (!needsRocket) {
            return;
        }

        if (ComboMode && Bool(QMenu, "Qcombo") &&
            (player.ManaPercent() > static_cast<float>(Slider(QMenu, "QmanaCombo", 10)) ||
             aaDamage * static_cast<float>(Slider(QMenu, "QmanaIgnore", 3)) > target.Health())) {
            Q.Cast();
            return;
        }

        const std::string key = MenuKey("harassQ", target);
        if (Orbwalker::ActiveMode() == OrbwalkingMode::Harass &&
            FarmMode &&
            Orbwalker::CanAttack() &&
            !player.Spellbook().IsWindingUp() &&
            Bool(QMenu, key.c_str()) &&
            Bool(QMenu, "Qharass") &&
            (player.ManaPercent() > static_cast<float>(Slider(QMenu, "QmanaHarass", 40)) ||
             aaDamage * static_cast<float>(Slider(QMenu, "QmanaIgnore", 3)) > target.Health())) {
            Q.Cast();
        }
        return;
    }

    if (ComboMode && player.ManaPercent() > static_cast<float>(Slider(QMenu, "QmanaCombo", 10))) {
        Q.Cast();
        return;
    }

    if (FarmMode &&
        !player.Spellbook().IsWindingUp() &&
        Bool(QMenu, "farmQout") &&
        Orbwalker::CanAttack()) {
        for (const auto& minion : GameObjects::EnemyLaneMinions()) {
            if (!ValidTarget(minion, Q.Range + 30.0f) ||
                AutoAttack::InAutoAttackRange(minion) ||
                RealPowPowRange(minion) >= RealDistance(minion) ||
                Q.Range >= RealDistance(minion)) {
                continue;
            }

            const float hp = SDK::HealthPrediction::GetPrediction(minion, 400, 70);
            const float aaDamage = SDK::Damage::GetAutoAttackDamage(player, minion);
            if (hp > 5.0f && hp < aaDamage * 1.2f) {
                Orbwalker::ForceTarget(minion);
                Q.Cast();
                return;
            }
        }
    }

    if (Orbwalker::ActiveMode() != OrbwalkingMode::LaneClear ||
        player.ManaPercent() <= static_cast<float>(Slider(QMenu, "QmanaLC", 80))) {
        return;
    }

    if (!IsLaneMinionTarget(orbTarget)) {
        return;
    }

    const AIBaseClient minion(orbTarget.Handle());
    if (CountLaneMinionsInRange(250.0f, minion.Position()) >= Slider(QMenu, "Qlaneclear", 2)) {
        Q.Cast();
    }
}

static void WLogic() {
    const auto player = Player();
    if (!player.IsValid() || !W.IsReady()) {
        return;
    }

    const auto target = GetTarget(W.Range, DamageType::Physical);
    if (Key(WMenu, "useW") && ValidHeroTarget(target, W.Range)) {
        CastSkillshot(W, target, HitChance::High, true);
        return;
    }

    if (!ValidHeroTarget(target, W.Range) || !WValidRange(target)) {
        return;
    }

    if (Bool(WMenu, "Wks") &&
        KsDamage(target, W) > target.Health() + target.AllShield()) {
        CastSkillshot(W, target, HitChance::High, true);
        return;
    }

    if (ComboMode &&
        Bool(WMenu, "Wcombo") &&
        player.ManaPercent() > static_cast<float>(Slider(WMenu, "WmanaCombo", 20))) {
        CastSkillshot(W, target, HitChance::High, true);
        return;
    }

    if (!FarmMode ||
        !Orbwalker::CanAttack() ||
        player.Spellbook().IsWindingUp() ||
        !Bool(WMenu, "Wharass") ||
        player.ManaPercent() <= static_cast<float>(Slider(WMenu, "WmanaHarass", 40))) {
        return;
    }

    if (ListIndex(WMenu, "Wts", 0) == 0) {
        const std::string key = MenuKey("harassW", target);
        if (Bool(WMenu, key.c_str())) {
            CastSkillshot(W, target, HitChance::High, true);
        }
        return;
    }

    // Wts == "All in range": in Jinx.cs this branch iterated an empty `Enemies`
    // list (no-op). Matching the original behavior — only the "Target selector"
    // mode (Wts == 0) above casts W in harass.
}

static void ELogic() {
    const auto player = Player();
    if (!player.IsValid() || !E.IsReady()) {
        return;
    }

    if (Key(EMenu, "useE")) {
        const auto target = GetTarget(E.Range, DamageType::Physical);
        if (ValidHeroTarget(target, E.Range)) {
            CastSkillshot(E, target, HitChance::High, false);
            return;
        }
    }

    if (ComboMode && Bool(EMenu, "Ecombo")) {
        const auto target = GetTarget(E.Range, DamageType::Magical);
        if (ValidHeroTarget(target, E.Range)) {
            const auto pred = E.GetPrediction(target);
            if (HasHardCc(target) || pred.Hitchance == HitChance::Immobile ||
                (HitchanceAtLeast(pred.Hitchance, HitChance::VeryHigh) &&
                 pred.GetCastPosition().Distance2D(target.Position()) > 200.0f)) {
                CastPosition(E, pred.GetCastPosition(), target);
                return;
            }
        }
    }

    // "E on CC" (Ecc): in Jinx.cs this loop iterated an empty `Enemies` list, so
    // it never executed (and it called E.GetPrediction per enemy every tick here).
    // Matching the original behavior — no auto-E on CC.

    if (Bool(EMenu, "Etel")) {
        for (const auto& obj : GameObjects::Enemy()) {
            if (!obj.IsValid() || obj.Position().Distance2D(player.Position()) > E.Range) {
                continue;
            }

            if (obj.HasBuff("teleport_target") || obj.HasBuff("Pantheon_GrandSkyfall_Jump")) {
                CastPosition(E, obj.Position(), obj);
                return;
            }
        }
    }
}

static void RLogic() {
    const auto player = Player();
    if (!player.IsValid() || !R.IsReady()) {
        return;
    }

    if (Key(RMenu, "useR")) {
        const auto target = GetTarget(R.Range, DamageType::Physical);
        if (ValidHeroTarget(target, R.Range)) {
            CastSkillshot(R, target, HitChance::High, false);
            return;
        }
    }

    const auto target = GetTarget(R.Range, DamageType::Physical);
    if (Bool(RMenu, "Rks") &&
        ValidHeroTarget(target, R.Range) &&
        target.DistanceToPlayer() > W.Range + 100.0f &&
        KsDamage(target, R) > target.Health() + target.AllShield()) {
        CastSkillshot(R, target, HitChance::High, false);
        return;
    }

    // Combo R (ComboRTeam / ComboRSolo): in Jinx.cs this loop iterated an empty
    // `Enemies` list inside `if (Combo && R.IsReady())`, so it never executed.
    // Matching the original behavior — no auto-R in combo. Only the Semi-cast
    // (useR) and Rks branches above remain active (both work in Jinx.cs).
}

static void BuildMenu() {
    MenuRoot = new Menu("champion.7upaio", "7UP - Jinx", true);

    QMenu = MenuRoot->AddSubMenu(new Menu("QMenu", "QMenu"));
    QMenu->Add(new MenuBool("Qcombo", "Combo Q"));
    QMenu->Add(new MenuBool("Qharass", "Harass Q"));
    QMenu->Add(new MenuBool("farmQout", "Farm Q out range AA minion"));
    QMenu->Add(new MenuSlider("Qlaneclear", "Lane clear x minions", 2, 1, 10));
    QMenu->Add(new MenuList("Qchange", "Q change mode FishBone -> MiniGun", { "Real Time", "Before AA" }, 1));
    QMenu->Add(new MenuSlider("Qaoe", "Force FishBone if can hit x target", 3, 0, 5));
    QMenu->Add(new MenuSlider("QmanaIgnore", "Ignore mana if can kill in x AA", 3, 0, 10));
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        const std::string key = MenuKey("harassQ", enemy);
        const std::string label = "Harass Q enemy: " + enemy.CharacterName();
        QMenu->Add(new MenuBool(key.c_str(), label.c_str()));
    }
    QMenu->Add(new MenuSlider("QmanaCombo", "Q combo mana", 10, 0, 100));
    QMenu->Add(new MenuSlider("QmanaHarass", "Q harass mana", 40, 0, 100));
    QMenu->Add(new MenuSlider("QmanaLC", "Q lane clear mana", 80, 0, 100));

    WMenu = MenuRoot->AddSubMenu(new Menu("WMenu", "WMenu"));
    WMenu->Add(new MenuKeyBind("useW", "Semi cast W key", SDK::Keys::S, KeyBindType::Press));
    WMenu->Add(new MenuBool("Wcombo", "Combo W"));
    WMenu->Add(new MenuBool("Wharass", "W harass"));
    WMenu->Add(new MenuBool("Wks", "W KS"));
    WMenu->Add(new MenuList("Wts", "Harass mode", { "Target selector", "All in range" }, 0));
    WMenu->Add(new MenuList("Wmode", "W mode", { "Out range MiniGun", "Out range FishBone", "Custome range" }, 0));
    WMenu->Add(new MenuSlider("Wcustome", "Custome minimum range", 600, 0, 1500));
    for (const auto& enemy : GameObjects::EnemyHeroes()) {
        const std::string key = MenuKey("harassW", enemy);
        const std::string label = "Harass W enemy: " + enemy.CharacterName();
        WMenu->Add(new MenuBool(key.c_str(), label.c_str()));
    }
    WMenu->Add(new MenuSlider("WmanaCombo", "W combo mana", 20, 0, 100));
    WMenu->Add(new MenuSlider("WmanaHarass", "W harass mana", 40, 0, 100));

    EMenu = MenuRoot->AddSubMenu(new Menu("EMenu", "EMenu"));
    EMenu->Add(new MenuBool("Ecombo", "Combo E"));
    EMenu->Add(new MenuBool("AutoEWhenEnemyCastAAM", "Use Auto E When Melee Enemy Cast AA On Me"));
    EMenu->Add(new MenuKeyBind("useE", "Semi cast E key", SDK::Keys::G, KeyBindType::Press));
    EMenu->Add(new MenuBool("Etel", "E on enemy teleport"));
    EMenu->Add(new MenuBool("Ecc", "E on CC"));
    EMenu->Add(new MenuBool("Espell", "E on special spell detection", false));
    EMenu->Add(new MenuSlider("EmanaCombo", "E mana", 30, 0, 100));
    EMenu->Add(new MenuBool("E Gap", "E Gap"));

    RMenu = MenuRoot->AddSubMenu(new Menu("RMenu", "RMenu"));
    RMenu->Add(new MenuBool("Rks", "R KS"));
    RMenu->Add(new MenuKeyBind("useR", "Semi-manual cast R key", SDK::Keys::T, KeyBindType::Press));
    RMenu->Add(new MenuBool("ComboRTeam", "Use R|Team Fight"));
    RMenu->Add(new MenuBool("ComboRSolo", "Use R|Solo Mode"));
    RMenu->Add(new MenuSlider("rMenuMin", "Use R| Min Range >= x", 1000, 500, 2500));
    RMenu->Add(new MenuSlider("rMenuMax", "Use R| Max Range <= x", 3000, 1500, 3500));

    DrawMenu = MenuRoot->AddSubMenu(new Menu("Drawing", "Drawing"));
    DrawMenu->Add(new MenuBool("DrawQ", "Draw Q"));
    DrawMenu->Add(new MenuBool("DrawW", "Draw W"));
    DrawMenu->Add(new MenuBool("DrawE", "Draw E"));
    DrawMenu->Add(new MenuBool("DrawR", "Draw R"));
    DrawMenu->Add(new MenuBool("onlyRdy", "Draw only ready spells"));
    DrawMenu->Add(new MenuBool("noti", "Show notification"));

    MenuRoot->Attach();
}

static void Orbwalker_OnAfterAttack(OrbwalkingActionArgs& args) {
    if (!FishBoneActive || !Q.IsReady()) {
        return;
    }

    const AIBaseClient target(args.Target.Handle());
    if (target.IsHero() && ListIndex(QMenu, "Qchange", 1) == 1 && ValidTarget(target)) {
        FishBoneToMiniGun(target);
        return;
    }

    if (ComboMode || !target.IsMinion()) {
        return;
    }

    if (Orbwalker::ActiveMode() == OrbwalkingMode::LaneClear &&
        Player().ManaPercent() > static_cast<float>(Slider(QMenu, "QmanaLC", 80)) &&
        CountLaneMinionsInRange(250.0f, target.Position()) >= Slider(QMenu, "Qlaneclear", 2)) {
        return;
    }

    if (RealDistance(target) < RealPowPowRange(target)) {
        Q.Cast();
    }
}

static void OnDoCast(const Events::ProcessSpellEventArgs& args) {
    if (!Loaded) {
        return;
    }

    const auto player = Player();
    if (!player.IsValid()) {
        return;
    }

    if (Events::IsLocalPlayer(args.Sender)) {
        const char* name = args.SpellName[0] ? args.SpellName : args.MissileName;
        if (EqualsIgnoreCase(name, "JinxWMissile") || EqualsIgnoreCase(name, "JinxW")) {
            WCastTime = Game::Time();
        }
        return;
    }

    if (!E.IsReady() || !args.Sender.IsValid()) {
        return;
    }

    const AIBaseClient sender(args.Sender.Ptr);
    if (!ValidTarget(sender, E.Range + 100.0f)) {
        return;
    }

    const bool targetIsPlayer =
        Events::IsLocalPlayer(args.Target) ||
        args.TargetNetworkId == player.NetworkId();

    if (Bool(EMenu, "Espell") &&
        player.ManaPercent() >= static_cast<float>(Slider(EMenu, "EmanaCombo", 30)) &&
        DangerousSpellName(args.SpellName[0] ? args.SpellName : args.ScriptName)) {
        CastPosition(E, sender.Position(), sender);
        return;
    }

    if (!targetIsPlayer || !args.IsAutoAttack) {
        return;
    }

    if (Bool(EMenu, "AutoEWhenEnemyCastAAM") &&
        sender.IsMelee() &&
        player.Distance(sender) < 300.0f) {
        CastPosition(E, player.Position(), sender);
        return;
    }

    if (ComboMode && Bool(EMenu, "Ecombo")) {
        if (player.Distance(sender) < 300.0f) {
            CastPosition(E, player.Position(), sender);
        } else if (player.Distance(sender) <= E.Range &&
                   player.CountEnemyHeroesInRange(300.0f) == 0) {
            CastSkillshot(E, sender, HitChance::High, false);
        }
    }
}

static void OnGapcloser(const GapCloserEventArgs& args) {
    if (!Loaded || !Bool(EMenu, "E Gap") || !E.IsReady()) {
        return;
    }

    const auto sender = AIHeroClient(args.Sender);
    if (ValidHeroTarget(sender, E.Range)) {
        CastPosition(E, args.End, sender);
    }
}

static void OnTeleport(const Events::Teleport::TeleportEventArgs& args) {
    if (!Loaded || !Bool(EMenu, "Etel") || !E.IsReady() ||
        args.Status != TeleportStatus::Start ||
        !args.IsTarget) {
        return;
    }

    const auto player = Player();
    const AIBaseClient target(args.Object);
    if (player.IsValid() && target.IsValid() &&
        target.Position().Distance2D(player.Position()) <= E.Range) {
        CastPosition(E, target.Position(), target);
    }
}

static void Game_OnUpdate(const GameUpdateEventArgs&) {
    if (!ShouldRunNow(LastUpdateTick, 40)) {
        return;
    }

    const auto player = Player();
    if (!player.IsValid() || player.IsDead() || player.IsRecalling() || Game::IsChatOpen()) {
        return;
    }

    if (R.Level() > 0) {
        R.Range = static_cast<float>(Slider(RMenu, "rMenuMax", 3000));
    }

    UpdateState();

    if (Q.IsReady() && ShouldRunNow(LastQTick, 60)) {
        QLogic();
    }
    if (W.IsReady() && ShouldRunNow(LastWTick, 90)) {
        WLogic();
    }
    if (E.IsReady() && ShouldRunNow(LastETick, 90)) {
        ELogic();
    }
    if (R.IsReady() && ShouldRunNow(LastRTick, 120)) {
        RLogic();
    }
}

static void OnGameLoad() {
    const auto player = Player();
    if (!player.IsValid() || Loaded) {
        return;
    }

    Q = Spell(SpellSlot::Q);
    W = Spell(SpellSlot::W, 1500.0f);
    E = Spell(SpellSlot::E, 925.0f);
    R = Spell(SpellSlot::R, 3000.0f);

    W.SetSkillshot(0.6f, 57.0f, 3300.0f, true, SpellType::Line);
    E.SetSkillshot(1.2f, 100.0f, 1750.0f, false, SpellType::Circle);
    R.SetSkillshot(0.6f, 130.0f, 1700.0f, false, SpellType::Line);

    BuildMenu();

    Events::hook.OnGameUpdate += &Game_OnUpdate;
    Orbwalker::OnAfterAttack += &Orbwalker_OnAfterAttack;
    Events::hook.OnDoCast += &OnDoCast;
    Events::hook.OnGapCloser += &OnGapcloser;
    Events::hook.OnTeleport += &OnTeleport;

    Loaded = true;
    Game::Print("<font color='#b756c5' size='20'>7UP - Jinx loaded</font>");
}

static void OnUnload() {
    if (!Loaded) {
        return;
    }

    Events::hook.OnGameUpdate -= &Game_OnUpdate;
    Orbwalker::OnAfterAttack -= &Orbwalker_OnAfterAttack;
    Events::hook.OnDoCast -= &OnDoCast;
    Events::hook.OnGapCloser -= &OnGapcloser;
    Events::hook.OnTeleport -= &OnTeleport;

    Loaded = false;
}

} // namespace Plugins::AIO7UP::Jinx
