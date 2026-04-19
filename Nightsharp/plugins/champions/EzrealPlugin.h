#pragma once

#include "../IPlugin.h"
#include "menu/MenuUI.h"
#include "sdk/Core/Game.h"
#include "sdk/SDK.h"
#include "sdk/Utils/Jungle.h"
#include "sdk/Wrappers/Damages/Damage.h"
#include "sdk/Wrappers/Orbwalking/Orbwalker.h"
#include "sdk/Wrappers/Spells/Spell.h"
#include "sdk/Wrappers/TargetSelector/TargetSelector.h"
#include "core/CrashTelemetry.h"

#include <cstdio>

namespace Plugins {

using namespace SDK;
using namespace SDK::MenuUI;

class EzrealPlugin : public IPlugin {
public:
    const char *GetName()       const override { return "Ezreal"; }
    const char *GetInternalId() const override { return "champion_ezreal"; }
    const char *GetAuthor()     const override { return "7UP / NightSharp"; }
    PluginCategory GetCategory() const override { return PluginCategory::Champion; }
    bool AutoLoadByDefault()    const override { return false; }

    bool CanLoad() const override {
        return Player().IsValid() && Player().CharacterName() == "Ezreal";
    }

    Spell Q, W, E, R, EQ;

    void OnLoad() override {
        if (m_menu) return;

        Q = Spell(SpellSlot::Q, 1200.0f);
        Q.SetSkillshot(0.25f, 53.0f, 2000.0f, true, SpellType::Line);

        W = Spell(SpellSlot::W, 1200.0f);
        W.SetSkillshot(0.25f, 55.0f, 1700.0f, false, SpellType::Line);

        E = Spell(SpellSlot::E, 475.0f);
        E.Delay = 0.65f;

        R = Spell(SpellSlot::R, 5000.0f);
        R.SetSkillshot(1.0f, 160.0f, 2200.0f, false, SpellType::Line);

        EQ = Spell(SpellSlot::Q, 1625.0f);
        EQ.SetSkillshot(0.90f, 57.0f, 1350.0f, true, SpellType::Line);

        m_menu = Menu::Create("EzrealRoot", "[NightSharp] Ezreal");

        auto *combo = m_menu->AddSubMenu("combo", "Combo Settings");
        combo->Add<MenuBool>("useQ", "Use Q", true);
        combo->Add<MenuBool>("useW", "Use W", true);
        combo->Add<MenuBool>("useE", "Use E", true);
        combo->Add<MenuBool>("ComboECheck", "Use E |Safe Check", true);
        combo->Add<MenuBool>("ComboEWall", "Use E |Wall Check", true);
        combo->Add<MenuBool>("useR", "Use R", true);
        combo->Add<MenuKeyBind>("SemiR", "Semi R", 'T', KeyBindType::Press);

        auto *harass = m_menu->AddSubMenu("harass", "Harass Settings");
        harass->Add<MenuBool>("useQ", "Use Q", true);
        harass->Add<MenuBool>("useW", "Use W", true);

        auto *laneclear = m_menu->AddSubMenu("laneclear", "LaneClear Settings");
        laneclear->Add<MenuBool>("useQ", "Use Q", true);
        laneclear->Add<MenuBool>("QLH", "Use Q Last Hit", false);
        laneclear->Add<MenuSlider>("ManaCL", "Mana Clear", 15, 0, 100);

        auto *jungle = m_menu->AddSubMenu("jungle", "Jungle Settings");
        jungle->Add<MenuBool>("useQ", "Use Q", true);
        jungle->Add<MenuBool>("useW", "Use W", true);
        jungle->Add<MenuSlider>("ManaCL", "Mana Clear", 15, 0, 100);

        auto *rmenu = m_menu->AddSubMenu("rmenu", "R Settings");
        rmenu->Add<MenuBool>("AutoR", "Auto R", true);
        rmenu->Add<MenuSlider>("RRange", "Auto R |Min Cast Range >= x", 900, 0, 1500);
        rmenu->Add<MenuSlider>("RMaxRange", "Auto R |Max Cast Range >= x", 3000, 1500, 5000);

        auto *misc = m_menu->AddSubMenu("misc", "Misc Settings");
        misc->Add<MenuBool>("gapcloser", "Gapcloser", true);
        misc->Add<MenuBool>("antigrab", "Anti-Hook (Blitz/Thresh/Pyke)", true);

        auto *ks = m_menu->AddSubMenu("killsteal", "KillSteal Settings");
        ks->Add<MenuBool>("killstealQ", "Use Q", true);
    }

    void OnUnload() override {
        if (!m_menu) return;
        Menu::Remove("EzrealRoot");
        m_menu = nullptr;
    }

    Menu *GetMenuRoot() override { return m_menu; }

    void OnBeforeAttack(OrbwalkingActionArgs& args) override {
        if (!m_menu || !args.Target.IsValid() || args.Target.IsDead()) return;

        if (Orbwalker::GetMode() == OrbwalkerMode::Combo) {
            auto *combo = m_menu->GetSubMenu("combo");
            if (combo && combo->GetBoolValue("useW", true) && W.IsReady()) {
                auto target = TargetSelector::GetTarget(W.Range, DamageType::Physical);
                if (target.IsValid() && target.IsValidTarget(W.Range)) {
                    W.CastPredicted(target, HitChance::High);
                }
            }
        }
    }

    void OnAfterAttack(OrbwalkingActionArgs& args) override {
        if (!m_menu || !args.Target.IsValid() || args.Target.IsDead()) return;

        const auto mode = Orbwalker::GetMode();

        if (mode == OrbwalkerMode::Combo) {
            auto *combo = m_menu->GetSubMenu("combo");
            if (combo && combo->GetBoolValue("useQ", true) && Q.IsReady()) {
                auto target = TargetSelector::GetTarget(Q.Range, DamageType::Physical);
                if (target.IsValid() && target.IsValidTarget(Q.Range)) {
                    Q.CastPredicted(target, HitChance::High);
                }
            }
        } else if (mode == OrbwalkerMode::Harass || mode == OrbwalkerMode::Clear) {
            auto *harass = m_menu->GetSubMenu("harass");
            if (harass && harass->GetBoolValue("useQ", true) && Q.IsReady()) {
                auto target = TargetSelector::GetTarget(Q.Range, DamageType::Physical);
                if (target.IsValid() && target.IsValidTarget(Q.Range)) {
                    Q.CastPredicted(target, HitChance::High);
                }
            }
        }
    }

    void OnGapcloser(const AIHeroClient& sender, const AntiGapcloser::GapcloserArgs& args) override {
        if (!m_menu) return;
        auto *misc = m_menu->GetSubMenu("misc");
        if (!misc || !misc->GetBoolValue("gapcloser", true)) return;
        if (!E.IsReady() || !sender.IsValid() || !sender.IsValidTarget(E.Range)) return;

        const Vector3 playerPos = Player().Position();
        const Vector3 senderPos = sender.Position();

        if (sender.IsMelee()) {
            if (sender.IsValidTarget(sender.AttackRange() + sender.BoundingRadius() + 100.0f, playerPos)) {
                E.Cast(playerPos + (playerPos - senderPos).Normalized() * E.Range);
                return;
            }
        }

        if (sender.IsDashing()) {
            if (args.EndPosition.Distance(playerPos) <= 250.0f ||
                senderPos.Distance(playerPos) <= 300.0f) {
                E.Cast(playerPos + (playerPos - senderPos).Normalized() * E.Range);
                return;
            }
        }

        if (senderPos.Distance(playerPos) <= 300.0f) {
            E.Cast(playerPos + (playerPos - senderPos).Normalized() * E.Range);
        }
    }

    void OnBuffGain(const AIBaseClient& sender, const SDK::Events::BuffEventArgs& args) override {
        if (!m_menu || !sender.IsValid() || !sender.IsMe() || !E.IsReady()) return;

        auto *misc = m_menu->GetSubMenu("misc");
        if (!misc || !misc->GetBoolValue("antigrab", true)) return;

        if (args.Name == "ThreshQ" || args.Name == "rocketgrab2" || args.Name == "PykeQ") {
            const Vector3 playerPos = Player().Position();

            AIHeroClient hookCaster;
            for (const auto& enemy : ObjectManager::EnemyHeroes()) {
                if (!enemy.IsValid()) continue;
                const std::string name = enemy.CharacterName();
                if ((args.Name == "ThreshQ" && name == "Thresh") ||
                    (args.Name == "rocketgrab2" && name == "Blitzcrank") ||
                    (args.Name == "PykeQ" && name == "Pyke")) {
                    hookCaster = enemy;
                    break;
                }
            }

            Vector3 escapeDir = hookCaster.IsValid()
                ? (playerPos - hookCaster.Position())
                : (playerPos - Player().Direction());

            float len = escapeDir.Length();
            if (len > 0.001f) escapeDir = escapeDir * (E.Range / len);
            E.Cast(playerPos + escapeDir);
        }
    }

    // Stage-tracking helper: writes "[NightSharp][Ezreal] <stage>" into the
    // CrashTelemetry pipeline so when the game crashes we can see exactly which
    // sub-function ran last.
    static void EzStage(const char* name) {
        CrashTelemetry::SetStage(name);
        char buf[160] = {};
        std::snprintf(buf, sizeof(buf), "[NightSharp][Ezreal] %s\r\n", name ? name : "?");
        CrashTelemetry::AppendStageLine(buf);
    }

    // SEH-safe position-based cast.
    //
    // Guards:
    //  1. Rejects a cast whose target position is further than `spell.Range+50`
    //     from the local player (xz/2D distance). LoL's internal castSpellFn
    //     appears to crash in ntdll heap-validation when we push a position
    //     that overflows the spell's maximum reach, which is the observed
    //     Ezreal-Q/LaneClear crash signature (rip=ntdll+0x649E6, rdx=-6).
    //     This filter is a safety net in case upstream `IsValidTarget` became
    //     stale due to offset drift between patches.
    //  2. Wraps the actual `spell.Cast` in __try/__except so any remaining
    //     access violation is caught locally instead of propagating to the
    //     game loop.
    //
    // Logs detailed pre/post telemetry (target addr, cast pos, player pos,
    // computed distance, spell range) so the last coordinates that crashed
    // the game are preserved in ns_stage.txt.
    __declspec(noinline) static bool SafeCastPos(const SDK::Spell& spell,
                                                 const Vector3& pos,
                                                 uintptr_t targetAddr,
                                                 const char* tag) {
        const Vector3 playerPos = SDK::ObjectManager::Player().Position();
        const float dist = playerPos.Distance2D(pos);
        const float range = spell.Range;

        char stageBuf[256] = {};
        std::snprintf(stageBuf, sizeof(stageBuf),
            "%s:pre target=0x%llX pos=(%.1f,%.1f,%.1f) "
            "player=(%.1f,%.1f,%.1f) dist=%.1f range=%.1f",
            tag ? tag : "SafeCastPos",
            static_cast<unsigned long long>(targetAddr),
            pos.x, pos.y, pos.z,
            playerPos.x, playerPos.y, playerPos.z,
            dist, range);
        EzStage(stageBuf);

        // Guard #1: reject clearly out-of-range casts before poking the game.
        constexpr float kRangeTolerance = 50.0f;
        if (range > 0.0f && dist > range + kRangeTolerance) {
            std::snprintf(stageBuf, sizeof(stageBuf),
                "%s:REJECT_OOR dist=%.1f > range=%.1f+%.0f (skipping cast)",
                tag ? tag : "SafeCastPos", dist, range, kRangeTolerance);
            EzStage(stageBuf);
            return false;
        }

        // Guard #2: SEH wrap the actual cast so we survive any residual AV.
        bool ok = false;
        __try {
            ok = spell.Cast(pos);
        }
        __except (CrashTelemetry::ReportAndHandle(
            tag ? tag : "SafeCastPos", GetExceptionInformation())) {
            ok = false;
        }

        std::snprintf(stageBuf, sizeof(stageBuf),
            "%s:post ok=%d", tag ? tag : "SafeCastPos", ok ? 1 : 0);
        EzStage(stageBuf);
        return ok;
    }

    void OnUpdate() override {
        __try {
            OnUpdateImpl();
        }
        __except (CrashTelemetry::ReportAndHandle("EzrealPlugin::OnUpdate", GetExceptionInformation())) {
            return;
        }
    }

    void OnUpdateImpl() {
        EzStage("OnUpdate:enter");
        if (!Player().IsValid() || !m_menu) return;
        if (Player().IsDead() || Player().IsRecalling() || Player().IsWindingUp()) return;

        EzStage("OnUpdate:rangeUpdate");
        if (R.Instance().Level() > 0) {
            auto *rmenu = m_menu->GetSubMenu("rmenu");
            if (rmenu) R.Range = static_cast<float>(rmenu->GetSliderValue("RMaxRange", 3000));
        }

        auto *combo = m_menu->GetSubMenu("combo");
        if (combo && combo->GetKeyBindValue("SemiR")) {
            EzStage("OnUpdate:OneKeyCastR");
            OneKeyCastR();
        }

        auto *rmenu = m_menu->GetSubMenu("rmenu");
        if (rmenu && rmenu->GetBoolValue("AutoR", true) && R.IsReady() &&
            Player().CountEnemyHeroesInRange(1000) == 0) {
            EzStage("OnUpdate:AutoRLogic");
            AutoRLogic();
        }

        const auto mode = Orbwalker::GetMode();
        switch (mode) {
        case OrbwalkerMode::Combo:
            EzStage("OnUpdate:Combo");
            Combo();
            break;
        case OrbwalkerMode::Harass:
            EzStage("OnUpdate:Harass");
            Harass();
            break;
        case OrbwalkerMode::Clear:
            EzStage("OnUpdate:LaneClear");
            LaneClear();
            EzStage("OnUpdate:JungleClear");
            JungleClear();
            break;
        case OrbwalkerMode::LastHit:
            EzStage("OnUpdate:LastHit");
            LastHit();
            break;
        default:
            break;
        }

        EzStage("OnUpdate:KillSteal");
        KillSteal();
        EzStage("OnUpdate:done");
    }

private:
    Menu *m_menu = nullptr;

    void Combo() {
        EzStage("Combo:enter");
        auto *combo = m_menu->GetSubMenu("combo");
        if (!combo) { EzStage("Combo:NO_MENU"); return; }

        const bool useQ = combo->GetBoolValue("useQ", true);
        const bool useW = combo->GetBoolValue("useW", true);
        const bool useE = combo->GetBoolValue("useE", true);
        const bool useR = combo->GetBoolValue("useR", true);

        char dbg[160] = {};
        std::snprintf(dbg, sizeof(dbg),
            "Combo:flags useQ=%d useW=%d useE=%d useR=%d",
            useQ ? 1 : 0, useW ? 1 : 0, useE ? 1 : 0, useR ? 1 : 0);
        EzStage(dbg);

        auto target = TargetSelector::GetTarget(EQ.Range, DamageType::Physical);
        if (!target.IsValid() || !target.IsValidTarget(EQ.Range)) {
            EzStage("Combo:NO_TARGET (EQ.Range)");
            return;
        }
        std::snprintf(dbg, sizeof(dbg),
            "Combo:target net=%d dist=%.1f",
            target.NetworkId(), target.DistanceToPlayer());
        EzStage(dbg);

        if (useE && E.IsReady() && target.IsValidTarget(EQ.Range)) {
            EzStage("Combo:ComboE-call");
            ComboELogic(target);
        }

        if (useW && W.IsReady() && target.IsValidTarget(W.Range)) {
            EzStage("Combo:W-prediction");
            auto wPred = W.GetPrediction(target);
            std::snprintf(dbg, sizeof(dbg), "Combo:W-HC=%d", static_cast<int>(wPred.Hitchance));
            EzStage(dbg);
            if (wPred.Hitchance >= HitChance::High) {
                if (Q.IsReady()) {
                    auto qPred = Q.GetPrediction(target);
                    if (qPred.Hitchance >= HitChance::High) {
                        SafeCastPos(W, qPred.CastPosition, target.Address(), "Combo:W-on-Q");
                    }
                }
                if (Player().InAutoAttackRange(target)) {
                    SafeCastPos(W, wPred.CastPosition, target.Address(), "Combo:W-main");
                }
            }
        } else if (useW) {
            std::snprintf(dbg, sizeof(dbg),
                "Combo:W-skip ready=%d targetInRange=%d",
                W.IsReady() ? 1 : 0,
                target.IsValidTarget(W.Range) ? 1 : 0);
            EzStage(dbg);
        }

        if (useQ && Q.IsReady() && target.IsValidTarget(Q.Range)) {
            EzStage("Combo:Q-prediction");
            auto qp = Q.GetPrediction(target);
            std::snprintf(dbg, sizeof(dbg), "Combo:Q-HC=%d", static_cast<int>(qp.Hitchance));
            EzStage(dbg);
            if (qp.Hitchance >= HitChance::Medium) {
                SafeCastPos(Q, qp.CastPosition, target.Address(), "Combo:Q");
            }
        } else if (useQ) {
            std::snprintf(dbg, sizeof(dbg),
                "Combo:Q-skip ready=%d targetInRange=%d",
                Q.IsReady() ? 1 : 0,
                target.IsValidTarget(Q.Range) ? 1 : 0);
            EzStage(dbg);
        }

        if (useR && R.IsReady()) {
            if (Player().CountEnemyHeroesInRange(800) > 1) return;

            auto *rmenu = m_menu->GetSubMenu("rmenu");
            const float rMinRange = rmenu ? static_cast<float>(rmenu->GetSliderValue("RRange", 900)) : 900.0f;

            for (const auto &rTarget : ObjectManager::EnemyHeroes()) {
                if (!rTarget.IsValidTarget(R.Range)) continue;
                if (rTarget.DistanceToPlayer() < rMinRange) continue;

                const float rDmg = Damage::GetSpellDamage(Player(), rTarget, SpellSlot::R, DamageStage::Default);

                if (rTarget.Health() < rDmg && rTarget.DistanceToPlayer() > Q.Range + E.Range / 2.0f) {
                    R.CastPredicted(rTarget, HitChance::High);
                }

                if (rTarget.IsValidTarget(Q.Range + E.Range)) {
                    float totalDmg = rDmg;
                    if (Q.IsReady()) totalDmg += Damage::GetSpellDamage(Player(), rTarget, SpellSlot::Q, DamageStage::Default);
                    if (W.IsReady()) totalDmg += Damage::GetSpellDamage(Player(), rTarget, SpellSlot::W, DamageStage::Default);
                    if (totalDmg > rTarget.Health() + rTarget.HPRegenRate() * 2.0f) {
                        R.CastPredicted(rTarget, HitChance::High);
                    }
                }
            }
        }
    }

    void ComboELogic(const AIHeroClient &target) {
        auto *combo = m_menu->GetSubMenu("combo");
        if (!combo || !target.IsValid() || !target.IsValidTarget()) return;

        const bool ECheck = combo->GetBoolValue("ComboECheck", true);
        const bool EWall  = combo->GetBoolValue("ComboEWall", true);

        if (!ECheck) return;
        if (Player().CountEnemyHeroesInRange(1200.0f) > 2) return;

        const float aaRange = Player().AttackRange() + Player().BoundingRadius() + target.BoundingRadius();
        if (target.DistanceToPlayer() <= aaRange) return;

        const Vector3 playerPos = Player().Position();
        const Vector3 targetPos = target.Position();
        const Vector3 cursorPos = Game::CursorPos();
        const Vector3 castEPos  = playerPos + (targetPos - playerPos).Normalized() * 475.0f;

        if (targetPos.Distance(cursorPos) >= playerPos.Distance(cursorPos)) return;

        float eDmg = Damage::GetSpellDamage(Player(), target, SpellSlot::E, DamageStage::Default);
        float aaDmg = Damage::GetAutoAttackDamage(Player(), target);
        if (target.Health() < eDmg + aaDmg) {
            if (!EWall || !CoreAPI::NavGrid::IsWall(castEPos)) E.Cast(castEPos);
            return;
        }

        if (W.IsReady()) {
            float wDmg = Damage::GetSpellDamage(Player(), target, SpellSlot::W, DamageStage::Default);
            if (target.Health() < eDmg + wDmg &&
                targetPos.Distance(cursorPos) + 350.0f < playerPos.Distance(cursorPos)) {
                if (!EWall || !CoreAPI::NavGrid::IsWall(castEPos)) E.Cast(castEPos);
                return;
            }
        }

        if (Q.IsReady()) {
            float qDmg = Damage::GetSpellDamage(Player(), target, SpellSlot::Q, DamageStage::Default);
            if (target.Health() < eDmg + qDmg &&
                targetPos.Distance(cursorPos) + 300.0f < playerPos.Distance(cursorPos)) {
                if (!EWall || !CoreAPI::NavGrid::IsWall(castEPos)) E.Cast(castEPos);
            }
        }
    }

    void Harass() {
        auto *harass = m_menu->GetSubMenu("harass");
        if (!harass) return;

        if (harass->GetBoolValue("useQ", true) && Q.IsReady()) {
            auto target = TargetSelector::GetTarget(Q.Range, DamageType::Physical);
            if (target.IsValid() && target.IsValidTarget(Q.Range)) {
                Q.CastPredicted(target, HitChance::High);
            }
        }
    }

    void LaneClear() {
        EzStage("LaneClear:enter");
        auto *lc = m_menu->GetSubMenu("laneclear");
        if (!lc || !lc->GetBoolValue("useQ", true)) return;
        if (Player().ManaPercent() < static_cast<float>(lc->GetSliderValue("ManaCL", 15))) return;
        if (!Q.IsReady()) return;

        const float aaRange = Player().AttackRange() + Player().BoundingRadius();
        const bool lastHitOnly = lc->GetBoolValue("QLH", false);

        Vector3 pushPos{};
        float pushDist = 0.0f;
        bool hasPush = false;

        EzStage("LaneClear:loop");
        for (const auto &minion : ObjectManager::EnemyMinions()) {
            if (!minion.IsValid() || !minion.IsAlive()) continue;
            if (!minion.IsValidTarget(Q.Range)) continue;

            EzStage("LaneClear:GetSpellDamage");
            const float qDmg = Damage::GetSpellDamage(Player(), minion, SpellSlot::Q, DamageStage::Default);
            if (qDmg <= 0.0f) continue;

            const float minionDist = minion.DistanceToPlayer();
            const bool outOfAARange = minionDist > aaRange + minion.BoundingRadius() + 50.0f;
            const float hp = minion.Health();

            // Priority 1: execute candidate.
            // Farm with Q any minion that will die to it, whether it's OUT of
            // AA range (orbwalker can't reach) or INSIDE AA range but too
            // tanky to finish with a single basic attack. Run a collision-
            // aware prediction so the line-shot doesn't hit a front minion
            // and waste the mana.
            if (hp <= qDmg && (outOfAARange || hp > Player().GetAutoAttackDamage(minion))) {
                EzStage("LaneClear:ExecuteCast");
                auto qp = Q.GetPrediction(minion);
                char dbg[160] = {};
                std::snprintf(dbg, sizeof(dbg),
                    "LaneClear:Execute-HC=%d dist=%.0f hp=%.0f qDmg=%.0f oar=%d",
                    static_cast<int>(qp.Hitchance),
                    minionDist, hp, qDmg,
                    outOfAARange ? 1 : 0);
                EzStage(dbg);
                if (qp.Hitchance >= HitChance::High) {
                    SafeCastPos(Q, qp.CastPosition, minion.Address(), "LaneClear:Execute");
                    return;
                }
                // Prediction rejected (collision / impossible) — keep scanning.
            }

            // Priority 2: push candidate (only when last-hit mode is disabled).
            // Push intentionally relies on Q's line-collision so the missile
            // hits the front-most minion and shoves the wave; no prediction
            // needed here, but we still respect last-hit/OOR constraints.
            if (!lastHitOnly && outOfAARange && minionDist > pushDist) {
                pushDist = minionDist;
                pushPos = minion.Position();
                hasPush = true;
            }
        }

        if (hasPush) {
            EzStage("LaneClear:PushCast");
            SafeCastPos(Q, pushPos, 0, "LaneClear:Push");
        }
        EzStage("LaneClear:exit");
    }

    void JungleClear() {
        EzStage("JungleClear:enter");
        auto *jg = m_menu->GetSubMenu("jungle");
        if (!jg) return;

        const bool useQ = jg->GetBoolValue("useQ", true);
        const bool useW = jg->GetBoolValue("useW", true);
        if (Player().ManaPercent() < static_cast<float>(jg->GetSliderValue("ManaCL", 15))) return;

        EzStage("JungleClear:fetchMobs");
        auto mobs = ObjectManager::JungleMinions();
        if (mobs.empty()) return;

        EzStage("JungleClear:sort");
        std::sort(mobs.begin(), mobs.end(),
            [](const AIMinionClient& a, const AIMinionClient& b) {
                return Utils::Jungle::GetJungleType(a) > Utils::Jungle::GetJungleType(b);
            });

        if (useW && W.IsReady()) {
            EzStage("JungleClear:W-scan");
            for (const auto &mob : mobs) {
                if (!mob.IsValid() || !mob.IsAlive()) continue;
                if (!mob.IsValidTarget(W.Range)) continue;
                if (Utils::Jungle::GetJungleType(mob) >= JungleType::Legendary) {
                    EzStage("JungleClear:W-cast");
                    SafeCastPos(W, mob.Position(), mob.Address(), "JungleClear:W");
                    return;
                }
            }
        }

        if (useQ && Q.IsReady()) {
            EzStage("JungleClear:Q-pick");
            const auto &mob = mobs.front();
            if (mob.IsValid() && mob.IsAlive() && mob.IsValidTarget(Q.Range)) {
                EzStage("JungleClear:Q-prediction");
                auto pred = Q.GetPrediction(mob);
                if (pred.Hitchance >= HitChance::Medium) {
                    EzStage("JungleClear:Q-cast");
                    SafeCastPos(Q, pred.CastPosition, mob.Address(), "JungleClear:Q");
                }
            }
        }
        EzStage("JungleClear:exit");
    }

    void LastHit() {
        auto *lc = m_menu->GetSubMenu("laneclear");
        if (!lc) return;
        if (Player().ManaPercent() < static_cast<float>(lc->GetSliderValue("ManaCL", 15))) return;
        if (!Q.IsReady()) return;

        for (const auto &minion : ObjectManager::EnemyMinions()) {
            if (!minion.IsValidTarget(Q.Range)) continue;

            const float aaRange = Player().AttackRange() + Player().BoundingRadius() + minion.BoundingRadius();
            if (minion.DistanceToPlayer() > aaRange &&
                minion.Health() < Damage::GetSpellDamage(Player(), minion, SpellSlot::Q, DamageStage::Default)) {
                Q.CastPredicted(minion, HitChance::Medium);
                return;
            }
        }
    }

    void OneKeyCastR() {
        CoreAPI::Control::IssueMove(Game::CursorPos());
        if (!R.IsReady()) return;

        auto target = TargetSelector::GetTarget(R.Range, DamageType::Physical);
        if (target.IsValid() && target.IsValidTarget(R.Range)) {
            R.CastPredicted(target, HitChance::High);
        }
    }

    void AutoRLogic() {
        auto *rmenu = m_menu->GetSubMenu("rmenu");
        if (!rmenu) return;

        const float rMinRange = static_cast<float>(rmenu->GetSliderValue("RRange", 900));

        for (const auto &target : ObjectManager::EnemyHeroes()) {
            if (!target.IsValidTarget(R.Range) || target.DistanceToPlayer() < rMinRange) continue;

            float rDmg = Damage::GetSpellDamage(Player(), target, SpellSlot::R, DamageStage::Default);
            float qDmg = Damage::GetSpellDamage(Player(), target, SpellSlot::Q, DamageStage::Default);
            const float effectiveHp = target.Health() + target.HPRegenRate() * 2.0f;

            if (!target.IsMoving() && target.IsValidTarget(EQ.Range) && rDmg + qDmg * 3.0f >= effectiveHp) {
                R.Cast(target.Position());
                continue;
            }

            if (rDmg > effectiveHp) {
                R.CastPredicted(target, HitChance::High);
            }
        }
    }

    void KillSteal() {
        auto *ks = m_menu->GetSubMenu("killsteal");
        if (!ks || !ks->GetBoolValue("killstealQ", true) || !Q.IsReady()) return;

        for (const auto &target : ObjectManager::EnemyHeroes()) {
            if (!target.IsValidTarget(Q.Range)) continue;

            if (target.HasBuff("JudicatorIntervention") || target.HasBuff("kindredrnodeathbuff") ||
                target.HasBuff("UndyingRage") || target.HasBuff("FioraW") ||
                target.HasBuff("ChronoShift") || target.HasBuff("zhonyasringshield") ||
                target.HasBuff("BardRStasis") || target.HasBuff("MelW")) continue;

            const int qLevel = Q.Instance().Level();
            if (qLevel <= 0) continue;
            constexpr float qBase[6] = {0.0f, 20.0f, 45.0f, 70.0f, 95.0f, 120.0f};
            const float qDmg = Player().CalculatePhysicalDamage(target,
                qBase[std::min(qLevel, 5)] + 1.30f * Player().BonusAttackDamage());
            const float effectiveHp = target.Health() + target.AllShield();

            if (Player().Distance(target) > 150.0f) {
                if (effectiveHp <= qDmg) { Q.CastPredicted(target, HitChance::High); return; }
            } else {
                if (effectiveHp <= qDmg * 1.5f) { Q.CastPredicted(target, HitChance::High); return; }
            }
        }
    }
};
}
