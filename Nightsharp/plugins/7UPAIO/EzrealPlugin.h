#pragma once

#include "../IPlugin.h"
#include "../../core/CoreNavGrid.h"
#include "../../sdk/Core/Game.h"
#include "../../sdk/Enumerations/CollisionObjects.h"
#include "../../sdk/GameObjects/ObjectManager.h"
#include "../../sdk/UI/UI.h"
#include "../../sdk/Utils/Jungle.h"
#include "../../sdk/Wrappers/Orbwalking/Orbwalker.h"
#include "../../sdk/Wrappers/Spells/Spell.h"
#include "../../sdk/Wrappers/TargetSelector/TargetSelector.h"

#include <algorithm>
#include <cfloat>

namespace Plugins {
namespace SevenUPAIO {

class EzrealPlugin final : public IPlugin {
public:
    const char* GetName() const override { return "[7UP]Ezreal"; }
    const char* GetInternalId() const override { return "7upaio_ezreal"; }
    const char* GetAuthor() const override { return "7UP AIO"; }
    PluginCategory GetCategory() const override { return PluginCategory::Champion; }
    const char* GetChampionName() const override { return "Ezreal"; }
    bool AutoLoadByDefault() const override { return true; }

    void OnLoad() override {
        if (!m_menu) {
            InitializeSpells();
            BuildMenu();
        }
    }

    SDK::MenuUI::Menu* GetMenuRoot() override {
        return m_menu;
    }

    void OnUpdate() override {
        const auto player = Player();
        if (!player.IsValid() || player.IsDead() || player.IsRecalling() ||
            SDK::Game::IsChatOpen() || player.IsWindingUp()) {
            return;
        }

        if (RLevel() > 0) {
            m_r.Range = static_cast<float>(RSlider("RMaxRange", 3000));
        }

        if (ComboKey("SemiR", false)) {
            OneKeyCastR();
        }

        // ── Throttle always-on scans (AutoR + KillSteal) ─────────────────
        // Both iterate every enemy hero and call GetPrediction on each,
        // which is the dominant cost of Ezreal::OnUpdate in overlay
        // profiling (May/2026 report: PluginsUpdate stage spikes to
        // 60–125 ms). Running them at 10 Hz instead of 60+ Hz cuts the
        // amortized prediction load by ~6× while keeping the reaction
        // window (100 ms) safely below human reflex (~200 ms). Combo /
        // Harass / LaneClear stay un-throttled — those are user-triggered
        // and must feel instantaneous.
        static DWORD s_lastAutoScanTick = 0;
        const DWORD now = GetTickCount();
        const bool runAutoScan = (now - s_lastAutoScanTick) >= 100;
        if (runAutoScan) {
            s_lastAutoScanTick = now;
        }

        if (runAutoScan && RBool("AutoR", true) && m_r.IsReady() &&
            player.CountEnemyHeroesInRange(1000.0f) == 0) {
            AutoRLogic();
        }

        switch (SDK::Orbwalker::GetMode()) {
        case SDK::OrbwalkerMode::Combo:
            Combo();
            return;
        case SDK::OrbwalkerMode::Harass:
            Harass();
            break;
        case SDK::OrbwalkerMode::LaneClear:
            LaneClear();
            JungleClear();
            break;
        case SDK::OrbwalkerMode::LastHit:
            LastHit();
            break;
        default:
            break;
        }

        if (runAutoScan) {
            KillSteal();
        }
    }

    void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) override {
        if (!IsAliveTarget(args.Target)) {
            return;
        }

        if (SDK::Orbwalker::GetMode() != SDK::OrbwalkerMode::Combo || !args.Target.IsHero()) {
            return;
        }

        const SDK::AIHeroClient target(args.Target.Address());
        if (ComboBool("useW", true) && m_w.IsReady() && target.IsValidTarget(m_w.Range)) {
            const auto pred = m_w.GetPrediction(target);
            if (pred.Hitchance >= SDK::HitChance::High) {
                m_w.Cast(target, pred.CastPosition);
            }
        }
    }

    void OnAfterAttack(SDK::OrbwalkingActionArgs& args) override {
        if (!IsAliveTarget(args.Target) || !args.Target.IsHero()) {
            return;
        }

        const SDK::AIHeroClient target(args.Target.Address());
        const auto mode = SDK::Orbwalker::GetMode();
        if (mode == SDK::OrbwalkerMode::Combo) {
            if (ComboBool("useQ", true) && m_q.IsReady() && target.IsValidTarget(m_q.Range)) {
                m_q.CastPredicted(target, SDK::HitChance::High);
            }
        } else if (mode == SDK::OrbwalkerMode::Harass || mode == SDK::OrbwalkerMode::LaneClear) {
            if (HarassBool("useQ", true) && m_q.IsReady() && target.IsValidTarget(m_q.Range)) {
                m_q.CastPredicted(target, SDK::HitChance::High);
            }
        }
    }

    void OnGapcloser(const SDK::AIHeroClient& sender,
                     const SDK::AntiGapcloser::GapcloserArgs& args) override {
        const auto player = Player();
        if (!MiscBool("gapcloser", true) || !m_e.IsReady() || !sender.IsValidTarget(m_e.Range)) {
            return;
        }

        const auto ePos = player.PreviousPosition().Extend(sender.PreviousPosition(), -m_e.Range);

        if (sender.IsMelee() &&
            sender.IsValidTarget(sender.AttackRange() + sender.BoundingRadius() + 100.0f)) {
            m_e.Cast(ePos);
            return;
        }

        if (sender.IsDashing() &&
            (args.EndPosition.Distance2D(player.Position()) <= 250.0f ||
             sender.PreviousPosition().Distance2D(player.Position()) <= 300.0f)) {
            m_e.Cast(ePos);
            return;
        }

        if (sender.IsCastingImporantSpell() &&
            sender.PreviousPosition().Distance2D(player.Position()) <= 300.0f) {
            m_e.Cast(ePos);
        }
    }

private:
    void InitializeSpells() {
        m_q = SDK::Spell(SDK::SpellSlot::Q, 1200.0f);
        m_q.SetSkillshot(0.25f, 53.0f, 2000.0f, true, SDK::SpellType::Line);

        m_w = SDK::Spell(SDK::SpellSlot::W, 1200.0f);
        m_w.SetSkillshot(0.25f, 55.0f, 1700.0f, false, SDK::SpellType::Line);

        m_e = SDK::Spell(SDK::SpellSlot::E, 475.0f);
        m_e.Delay = 0.65f;

        m_r = SDK::Spell(SDK::SpellSlot::R, 5000.0f);
        m_r.SetSkillshot(1.0f, 160.0f, 2200.0f, false, SDK::SpellType::Line);

        m_eq = SDK::Spell(SDK::SpellSlot::Q, 1625.0f);
        m_eq.SetSkillshot(0.90f, 57.0f, 1350.0f, true, SDK::SpellType::Line);
    }

    void BuildMenu() {
        m_menu = SDK::Menu::Create("Ezreal", "[7UP]Ezreal");

        m_comboMenu = m_menu->AddSubMenu("Combo Settings", "Combo");
        m_comboMenu->Add<SDK::MenuBool>("useQ", "Use Q", true);
        m_comboMenu->Add<SDK::MenuBool>("useW", "Use W", true);
        m_comboMenu->Add<SDK::MenuBool>("useE", "Use E", true);
        m_comboMenu->Add<SDK::MenuBool>("ComboECheck", "Use E |Safe Check", true);
        m_comboMenu->Add<SDK::MenuBool>("ComboEWall", "Use E |Wall Check", true);
        m_comboMenu->Add<SDK::MenuBool>("useR", "Use R", true);
        m_comboMenu->Add<SDK::MenuKeyBind>("SemiR", "Semi R", 'T', SDK::KeyBindType::Press, false);

        m_harassMenu = m_menu->AddSubMenu("Harass Settings", "Harass");
        m_harassMenu->Add<SDK::MenuBool>("useQ", "Use Q", true);
        m_harassMenu->Add<SDK::MenuBool>("useW", "Use W", true);

        m_laneClearMenu = m_menu->AddSubMenu("LaneClear Settings", "Lane Clear");
        m_laneClearMenu->Add<SDK::MenuBool>("useQ", "Use Q", true);
        m_laneClearMenu->Add<SDK::MenuBool>("QLH", "Use Q Last Hit", false);
        m_laneClearMenu->Add<SDK::MenuSlider>("ManaCL", "Mana Clear", 15, 0, 100);

        m_jungleClearMenu = m_menu->AddSubMenu("Jungle Settings", "Jungle Clear");
        m_jungleClearMenu->Add<SDK::MenuBool>("useQ", "Use Q", true);
        m_jungleClearMenu->Add<SDK::MenuBool>("useW", "Use W", true);
        m_jungleClearMenu->Add<SDK::MenuSlider>("ManaCL", "Mana Clear", 15, 0, 100);

        m_rMenu = m_menu->AddSubMenu("R Settings", "RMenu");
        m_rMenu->Add<SDK::MenuBool>("AutoR", "Auto R", true);
        m_rMenu->Add<SDK::MenuSlider>("RRange", "Auto R |Min Cast Range >= x", 900, 0, 1500);
        m_rMenu->Add<SDK::MenuSlider>("RMaxRange", "Auto R |Max Cast Range >= x", 3000, 1500, 5000);

        m_miscMenu = m_menu->AddSubMenu("Misc Settings", "Misc");
        m_miscMenu->Add<SDK::MenuBool>("gapcloser", "Gapcloser", true);

        m_killStealMenu = m_menu->AddSubMenu("KillSteal Settings", "KillSteal");
        m_killStealMenu->Add<SDK::MenuBool>("killstealQ", "Use Q", true);
    }

    static bool IsAliveTarget(const SDK::GameObject& target) {
        return target.IsValid() && !target.IsDead() && target.IsValidTarget() && target.Health() > 0.0f;
    }

    static bool HasNoDeathBuff(const SDK::AIHeroClient& target) {
        return !target.HasBuff("JudicatorIntervention") &&
               !target.HasBuff("KayleR") &&
               !target.HasBuff("kindredrnodeathbuff") &&
               !target.HasBuff("Undying Rage") &&
               !target.HasBuff("UndyingRage") &&
               !target.HasBuff("Undying_Rage") &&
               !target.HasBuff("FioraW") &&
               !target.HasBuff("BlitzcrankManaBarrierCO") &&
               !target.HasBuff("BlitzcrankManaBarrierCD") &&
               !target.HasBuff("ChronoShift") &&
               !target.HasBuff("zhonyasringshield") &&
               !target.HasBuff("BardRStasis") &&
               !target.HasBuff("MelW");
    }

    void OneKeyCastR() {
        const auto player = Player();
        player.IssueOrder(SDK::GameObjectOrder::MoveTo, SDK::Game::CursorPos());

        if (!m_r.IsReady()) {
            return;
        }

        const auto target = SDK::TargetSelector::GetTarget(m_r.Range, SDK::DamageType::Physical);
        if (target.IsValidTarget(m_r.Range) && !target.IsValidTarget(static_cast<float>(RSlider("RRange", 900)))) {
            m_r.CastPredicted(target, SDK::HitChance::High);
        }
    }

    void AutoRLogic() {
        const auto player = Player();
        for (const auto& target : SDK::ObjectManager::EnemyHeroes()) {
            if (!target.IsValidTarget(m_r.Range) ||
                target.DistanceToPlayer() < static_cast<float>(RSlider("RRange", 900))) {
                continue;
            }

            const float targetHealth = target.Health() + target.HPRegenRate() * 2.0f;
            const float rDamage = player.GetSpellDamage(target, SDK::SpellSlot::R);

            if (!target.CanMove() && target.IsValidTarget(m_eq.Range) &&
                rDamage + player.GetSpellDamage(target, SDK::SpellSlot::Q) * 3.0f >= targetHealth) {
                m_r.Cast(target);
                return;
            }

            if (rDamage > targetHealth && target.GetPathLength() < 2 &&
                m_r.GetPrediction(target).Hitchance >= SDK::HitChance::High) {
                m_r.Cast(target);
                return;
            }
        }
    }

    void Combo() {
        const auto player = Player();
        auto target = SDK::TargetSelector::GetTarget(m_eq.Range, SDK::DamageType::Physical);
        if (!target.IsValidTarget(m_eq.Range)) {
            return;
        }

        if (ComboBool("useE", true) && m_e.IsReady() && target.IsValidTarget(m_eq.Range)) {
            ComboELogic(target);
        }

        // ── AA-friendly single-spell-per-tick gate ────────────────────────────
        // Casting both W and Q in the same tick fires two spells back-to-back
        // and cancels the queued AA windup (the "AA bị mất một nhịp" bug). The
        // OnBefore/OnAfter handlers already cover the AA-cycle W+Q pair, so
        // here we only fire ONE poke spell per tick — let the next tick (or
        // the AA event handlers) deliver the other.
        bool castedThisTick = false;

        if (ComboBool("useW", true) && m_w.IsReady() && target.IsValidTarget(m_w.Range)) {
            const auto wPred = m_w.GetPrediction(target);
            if (wPred.Hitchance >= SDK::HitChance::High) {
                if (m_q.IsReady()) {
                    const auto qPred = m_q.GetPrediction(target);
                    if (qPred.Hitchance >= SDK::HitChance::High) {
                        if (m_w.Cast(target, qPred.CastPosition)) {
                            castedThisTick = true;
                        }
                    }
                }

                if (!castedThisTick && SDK::Orbwalker::CanAttack() &&
                    player.InAutoAttackRange(target)) {
                    if (m_w.Cast(target, wPred.CastPosition)) {
                        castedThisTick = true;
                    }
                }
            }
        }

        if (!castedThisTick && ComboBool("useQ", true) && m_q.IsReady() &&
            target.IsValidTarget(m_q.Range)) {
            m_q.CastPredicted(target, SDK::HitChance::High);
        }

        if (!ComboBool("useR", true) || !m_r.IsReady()) {
            return;
        }

        if (player.IsUnderEnemyTurret() || player.CountEnemyHeroesInRange(800.0f) > 1) {
            return;
        }

        for (const auto& rTarget : SDK::ObjectManager::EnemyHeroes()) {
            if (!rTarget.IsValidTarget(m_r.Range) ||
                rTarget.DistanceToPlayer() < static_cast<float>(RSlider("RRange", 900))) {
                continue;
            }

            if (rTarget.Health() < player.GetSpellDamage(rTarget, SDK::SpellSlot::R) &&
                m_r.GetPrediction(rTarget).Hitchance >= SDK::HitChance::High &&
                rTarget.DistanceToPlayer() > m_q.Range + m_e.Range / 2.0f) {
                m_r.Cast(target);
                return;
            }

            if (rTarget.IsValidTarget(m_q.Range + m_e.Range) &&
                player.GetSpellDamage(rTarget, SDK::SpellSlot::R) +
                    (m_q.IsReady() ? player.GetSpellDamage(rTarget, SDK::SpellSlot::Q) : 0.0f) +
                    (m_w.IsReady() ? player.GetSpellDamage(rTarget, SDK::SpellSlot::W) : 0.0f) >
                    rTarget.Health() + rTarget.HPRegenRate() * 2.0f) {
                m_r.Cast(rTarget);
                return;
            }
        }
    }

    void ComboELogic(const SDK::AIHeroClient& target) {
        const auto player = Player();
        const bool safeCheck = ComboBool("ComboECheck", true);
        const bool wallCheck = ComboBool("ComboEWall", true);
        if (!target.IsValidTarget()) {
            return;
        }

        if (!safeCheck || player.IsUnderEnemyTurret() || player.CountEnemyHeroesInRange(1200.0f) > 2) {
            return;
        }

        if (target.DistanceToPlayer() <= player.GetRealAutoAttackRange(target)) {
            return;
        }

        const auto castEPos = player.PreviousPosition().Extend(target.PreviousPosition(), m_e.Range);
        const auto cursor = SDK::Game::CursorPos();
        const float targetCursorDist = target.PreviousPosition().Distance2D(cursor);
        const float playerCursorDist = player.PreviousPosition().Distance2D(cursor);

        const auto tryCastE = [&]() -> bool {
            if (wallCheck && CoreNavGrid::IsWall(castEPos)) {
                return false;
            }
            return m_e.Cast(castEPos);
        };

        if (target.Health() < player.GetSpellDamage(target, SDK::SpellSlot::E) + player.GetAutoAttackDamage(target) &&
            targetCursorDist < playerCursorDist) {
            tryCastE();
            return;
        }

        if (target.Health() < player.GetSpellDamage(target, SDK::SpellSlot::E) + player.GetSpellDamage(target, SDK::SpellSlot::W) &&
            m_w.IsReady() && targetCursorDist + 350.0f < playerCursorDist) {
            tryCastE();
            return;
        }

        if (target.Health() < player.GetSpellDamage(target, SDK::SpellSlot::E) + player.GetSpellDamage(target, SDK::SpellSlot::Q) &&
            m_q.IsReady() && targetCursorDist + 300.0f < playerCursorDist) {
            tryCastE();
        }
    }

    void Harass() {
        if (!HarassBool("useQ", true) || !m_q.IsReady()) {
            return;
        }

        const auto target = SDK::TargetSelector::GetTarget(m_q.Range, SDK::DamageType::Physical);
        if (target.IsValidTarget(m_q.Range)) {
            m_q.CastPredicted(target, SDK::HitChance::High);
        }
    }

    void LaneClear() {
        // Mirrors `old source/plugins/champions/EzrealPlugin.h::LaneClear()`:
        // ALWAYS prefer Q over an autoattack the moment a minion's HP drops to
        // within Q's damage. The previous "shouldQ" filter (turret-only / out-
        // of-AA-range / HP > AA dmg) refused to fire Q on any minion the AA
        // could already kill, so AA usually claimed the last hit and Q never
        // came out — exactly what the user reported.
        const auto player = Player();
        if (!LaneClearBool("useQ", true) ||
            player.ManaPercent() < static_cast<float>(LaneClearSlider("ManaCL", 15)) ||
            !m_q.IsReady()) {
            return;
        }

        const float aaRange = player.AttackRange() + player.BoundingRadius();
        const bool lastHitOnly = LaneClearBool("QLH", false);

        for (const auto& minion : SDK::ObjectManager::EnemyMinions()) {
            if (!minion.IsValid() || !minion.IsValidTarget(m_q.Range) || !minion.IsMinion()) {
                continue;
            }

            const float qDmg = m_q.GetDamage(minion);
            if (qDmg <= 0.0f) {
                continue;
            }

            const float hp = minion.Health();
            if (hp > qDmg) {
                continue;
            }

            if (lastHitOnly) {
                const bool outOfAARange =
                    minion.DistanceToPlayer() > aaRange + minion.BoundingRadius() + 50.0f;
                const bool aaCantKill = hp > player.GetAutoAttackDamage(minion);
                if (!outOfAARange && !aaCantKill) {
                    // Inside AA range AND AA alone would kill it — defer to AA.
                    continue;
                }
            }

            if (m_q.CastPredicted(minion, SDK::HitChance::Medium)) {
                return;
            }
        }
    }

    void JungleClear() {
        const auto player = Player();
        if (player.ManaPercent() < static_cast<float>(JungleSlider("ManaCL", 15))) {
            return;
        }

        const bool useQ = JungleBool("useQ", true);
        const bool useW = JungleBool("useW", true);
        auto mobs = SDK::ObjectManager::JungleMinions();
        std::sort(mobs.begin(), mobs.end(), [](const SDK::AIMinionClient& lhs, const SDK::AIMinionClient& rhs) {
            return lhs.MaxHealth() > rhs.MaxHealth();
        });

        for (const auto& obj : mobs) {
            if (!obj.IsValidTarget(m_q.Range)) {
                continue;
            }

            if (useW && m_w.IsReady() && obj.IsValidTarget(m_w.Range) &&
                static_cast<int>(SDK::Utils::Jungle::GetJungleType(obj)) >= static_cast<int>(SDK::JungleType::Legendary)) {
                if (m_w.CastPredicted(obj, SDK::HitChance::High, false, -1.0f,
                    { SDK::CollisionObjects::YasuoWall, SDK::CollisionObjects::Heroes })) {
                    return;
                }
            }

            if (useQ && m_q.IsReady()) {
                if (m_q.CastPredicted(obj, SDK::HitChance::Medium)) {
                    return;
                }
            }
        }
    }

    void LastHit() {
        const auto player = Player();
        if (player.ManaPercent() >= static_cast<float>(LaneClearSlider("ManaCL", 15)) || !m_q.IsReady()) {
            return;
        }

        for (const auto& minion : SDK::ObjectManager::EnemyMinions()) {
            if (!minion.IsValidTarget(m_q.Range) || !minion.IsMinion()) {
                continue;
            }
            if (minion.DistanceToPlayer() <= player.GetRealAutoAttackRange(minion)) {
                continue;
            }
            if (minion.Health() < player.GetSpellDamage(minion, SDK::SpellSlot::Q)) {
                m_q.CastPredicted(minion, SDK::HitChance::Medium);
                return;
            }
        }
    }

    float QDamage(const SDK::AIBaseClient& target) const {
        const int qLevel = QLevel();
        if (qLevel <= 0) {
            return 0.0f;
        }

        static constexpr float baseDamage[] = { 0.0f, 20.0f, 45.0f, 70.0f, 95.0f, 120.0f };
        const auto player = Player();
        const int idx = (std::min)(5, (std::max)(0, qLevel));
        return player.CalculateDamage(target, SDK::DamageType::Physical,
            baseDamage[idx] + 1.3f * player.BonusAttackDamage());
    }

    void KillSteal() {
        if (!KillBool("killstealQ", true) || !m_q.IsReady()) {
            return;
        }

        const auto player = Player();
        for (const auto& target : SDK::ObjectManager::EnemyHeroes()) {
            if (!target.IsValidTarget(m_w.Range) || !HasNoDeathBuff(target)) {
                continue;
            }

            const float threshold = player.Distance(target) > 150.0f ? QDamage(target) : QDamage(target) * 1.5f;
            if (target.Health() + target.AllShield() > threshold) {
                continue;
            }

            if (m_q.CastPredicted(target, SDK::HitChance::High)) {
                return;
            }
        }
    }

    int QLevel() const { return Player().GetSpell(SDK::SpellSlot::Q).Level(); }
    int RLevel() const { return Player().GetSpell(SDK::SpellSlot::R).Level(); }

    bool ComboBool(const char* name, bool fallback) const { return m_comboMenu ? m_comboMenu->GetBoolValue(name, fallback) : fallback; }
    bool HarassBool(const char* name, bool fallback) const { return m_harassMenu ? m_harassMenu->GetBoolValue(name, fallback) : fallback; }
    bool LaneClearBool(const char* name, bool fallback) const { return m_laneClearMenu ? m_laneClearMenu->GetBoolValue(name, fallback) : fallback; }
    bool JungleBool(const char* name, bool fallback) const { return m_jungleClearMenu ? m_jungleClearMenu->GetBoolValue(name, fallback) : fallback; }
    bool RBool(const char* name, bool fallback) const { return m_rMenu ? m_rMenu->GetBoolValue(name, fallback) : fallback; }
    bool MiscBool(const char* name, bool fallback) const { return m_miscMenu ? m_miscMenu->GetBoolValue(name, fallback) : fallback; }
    bool KillBool(const char* name, bool fallback) const { return m_killStealMenu ? m_killStealMenu->GetBoolValue(name, fallback) : fallback; }
    bool ComboKey(const char* name, bool fallback) const { return m_comboMenu ? m_comboMenu->GetKeyBindValue(name, fallback) : fallback; }
    int LaneClearSlider(const char* name, int fallback) const { return m_laneClearMenu ? m_laneClearMenu->GetSliderValue(name, fallback) : fallback; }
    int JungleSlider(const char* name, int fallback) const { return m_jungleClearMenu ? m_jungleClearMenu->GetSliderValue(name, fallback) : fallback; }
    int RSlider(const char* name, int fallback) const { return m_rMenu ? m_rMenu->GetSliderValue(name, fallback) : fallback; }

    SDK::Menu* m_menu = nullptr;
    SDK::Menu* m_comboMenu = nullptr;
    SDK::Menu* m_harassMenu = nullptr;
    SDK::Menu* m_laneClearMenu = nullptr;
    SDK::Menu* m_jungleClearMenu = nullptr;
    SDK::Menu* m_miscMenu = nullptr;
    SDK::Menu* m_killStealMenu = nullptr;
    SDK::Menu* m_rMenu = nullptr;

    SDK::Spell m_q = {};
    SDK::Spell m_w = {};
    SDK::Spell m_e = {};
    SDK::Spell m_r = {};
    SDK::Spell m_eq = {};
};

} // namespace SevenUPAIO
} // namespace Plugins
