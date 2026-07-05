#pragma once
// Port of OKTW_CSharp/Champions/Blitzcrank.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuKeyBind;
using SDK::UI::MenuSlider;

class BlitzcrankPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Blitzcrank"; }
    const char* GetInternalId() const override { return "champion.oktw.blitzcrank"; }
    const char* GetChampionName() const override { return "Blitzcrank"; }

protected:
    // Grab statistics (C#: private int grab, grabS; private float grabW)
    int   m_grab  = 0;
    int   m_grabS = 0;
    float m_grabW = 0.0f;

    void BuildMenu() override {
        MarkActive();

        m_Q = Spell(SpellSlot::Q, 920.0f);
        m_W = Spell(SpellSlot::W, 200.0f);
        m_E = Spell(SpellSlot::E, 475.0f);
        m_R = Spell(SpellSlot::R, 600.0f);

        // Q: Rocket Grab (line skillshot with collision)
        m_Q.SetSkillshot(0.25f, 80.0f, 1800.0f, true, SDK::SpellType::Line);

        // Champion-level toggles (C# adds these directly under Player.ChampionName menu)
        m_champMenu->Add(new MenuBool("autoW",    "Auto W", true));
        m_champMenu->Add(new MenuBool("autoE",    "Auto E", true));
        m_champMenu->Add(new MenuBool("showgrab", "Show statistics", true));

        // Q option submenu
        Menu* qOpt = m_qMenu->AddSubMenu(new Menu("QOption", "Q option"));
        qOpt->Add(new MenuBool("ts",  "Use common TargetSelector", true));
        qOpt->Add(new MenuBool("ts1", "ON - only one target", false));
        qOpt->Add(new MenuBool("ts2", "OFF - all grab-able targets", false));
        qOpt->Add(new MenuBool("qTur", "Auto Q under turret", true));
        qOpt->Add(new MenuBool("qCC",  "Auto Q cc & dash enemy", true));
        qOpt->Add(new MenuSlider("minGrab", "Min range grab", 250, 125, static_cast<int>(m_Q.Range)));
        qOpt->Add(new MenuSlider("maxGrab", "Max range grab", static_cast<int>(m_Q.Range), 125, static_cast<int>(m_Q.Range)));

        Menu* grabList = qOpt->AddSubMenu(new Menu("GrabSub", "Grab"));
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string id = std::string("grab") + enemy.CharacterName();
            grabList->Add(new MenuBool(id.c_str(), enemy.CharacterName().c_str(), true));
        }

        // R option submenu
        Menu* rOpt = m_rMenu->AddSubMenu(new Menu("ROption", "R option"));
        rOpt->Add(new MenuSlider("rCount",    "Auto R if enemies in range", 3, 0, 5));
        rOpt->Add(new MenuBool("afterGrab",   "Auto R after grab", true));
        rOpt->Add(new MenuBool("afterAA",     "Auto R befor AA", true));
        rOpt->Add(new MenuBool("rKs",         "R ks", false));
        rOpt->Add(new MenuBool("inter",       "OnPossibleToInterrupt", true));
        rOpt->Add(new MenuBool("Gap",         "OnEnemyGapcloser", true));

        // Draw submenu (extend the base draw menu)
        m_drawMenu->Add(new MenuBool("qRange",  "Q range", false));
        m_drawMenu->Add(new MenuBool("rRange",  "R range", false));
        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw when skill rdy", true));
    }

    void SetMana() override {
        if ((Shared().manaDisable && Shared().manaDisable->Value && Combo()) ||
            Player().HealthPercent() < 20.0f) {
            m_QMANA = m_WMANA = m_EMANA = m_RMANA = 0.0f;
            return;
        }
        m_QMANA = m_Q.Instance().ManaCost();
        m_WMANA = m_W.Instance().ManaCost();
        m_EMANA = m_E.Instance().ManaCost();
        m_RMANA = m_R.IsReady() ? m_R.Instance().ManaCost() : 0.0f;
    }

    void OnGameUpdate() override {
        const auto p = Player();
        if (!p.IsValid()) return;
        SetMana();

        if (LagFree(1) && m_Q.IsReady())
            LogicQ();
        if (LagFree(2) && m_R.IsReady())
            LogicR();
        if (LagFree(3) && m_W.IsReady() && GetBool("autoW"))
            LogicW();

        // After-AA R cast (C# ports the Orbwalking.AfterAttack event here since the
        // NightSharp SDK lacks a matching hook — evaluated each tick as a fallback).
        // TODO(oktw-port): wire real Orbwalker::AfterAttack event when available.
        if (GetBool("afterAA") && m_R.IsReady()) {
            auto* ts = SDK::TargetSelector::Instance();
            auto aaTarget = ts ? ts->GetTarget(p.AttackRange() + p.BoundingRadius(),
                                                DamageType::Physical)
                               : AIHeroClient();
            if (aaTarget.IsValid() &&
                SDK::Extensions::IsValidTarget(aaTarget, p.AttackRange() + p.BoundingRadius(), true)) {
                m_R.Cast();
            }
        }

        // BeforeAttack: auto-E when about to auto a hero
        // TODO(oktw-port): wire real Orbwalker::BeforeAttack event when available.
        if (m_E.IsReady() && GetBool("autoE")) {
            auto* ts = SDK::TargetSelector::Instance();
            auto eTarget = ts ? ts->GetTarget(p.AttackRange() + p.BoundingRadius(),
                                               DamageType::Physical)
                              : AIHeroClient();
            if (eTarget.IsValid() &&
                SDK::Extensions::IsValidTarget(eTarget, p.AttackRange() + p.BoundingRadius(), true)) {
                m_E.Cast();
            }
        }

        // Interrupter (TODO: SDK Interrupter event not yet available)
        // TODO(oktw-port): hook Interrupter2::OnInterruptableTarget → cast R if enemy channelling.
        if (GetBool("inter") && m_R.IsReady()) {
            // Passive fallback: no interrupter data — skip.
        }

        // AntiGapcloser (TODO: SDK AntiGapcloser event not yet available)
        // TODO(oktw-port): hook AntiGapcloser::OnEnemyGapcloser → cast R if enemy dashing to us.
        if (GetBool("Gap") && m_R.IsReady()) {
            // Passive fallback: no gapcloser data — skip.
        }

        // Grab statistics: when Q on cooldown, count enemies still carrying rocketgrab2
        if (!m_Q.IsReady() && (SDK::Game::Time() - m_grabW) > 2.0f) {
            for (const auto& t : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!t.IsValid() || !t.IsEnemy()) continue;
                if (t.HasBuff("rocketgrab2")) {
                    ++m_grabS;
                    m_grabW = SDK::Game::Time();
                    NightSharpDebug::Logf("[OKTW/Blitzcrank] GRAB!!!!");
                }
            }
        }

        // ProcessSpellCast fallback: increment grab count when RocketGrabMissile fires.
        // TODO(oktw-port): hook Obj_AI_Base::OnProcessSpellCast for a precise count.
    }

    void LogicQ() {
        const float maxGrab = static_cast<float>(GetSlider("maxGrab", static_cast<int>(m_Q.Range)));
        const float minGrab = static_cast<float>(GetSlider("minGrab", 250));
        const bool  ts      = GetBool("ts");
        const bool  qTur    = Player().IsUnderAllyTurret() && GetBool("qTur");
        const bool  qCC     = GetBool("qCC");
        const bool  countE  = OktwCommon::CountEnemiesInRange(Player().ServerPosition(), 1500.0f) == 1;

        if (Combo() && ts) {
            auto* tsel = SDK::TargetSelector::Instance();
            auto t = tsel ? tsel->GetTarget(maxGrab, DamageType::Physical) : AIHeroClient();
            // TODO(oktw-port): SpellShield/SpellImmunity buff-type check not available
            if (t.IsValid() &&
                SDK::Extensions::IsValidTarget(t, maxGrab, true) &&
                !false &&
                !false &&
                GetBool((std::string("grab") + t.CharacterName()).c_str()) &&
                Player().Position().Distance(t.ServerPosition()) > minGrab) {
                CastSpell(m_Q, t);
            }
        }

        for (const auto& t : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!t.IsValid() || !t.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(t, maxGrab, true)) continue;

            const bool grabTog = GetBool((std::string("grab") + t.CharacterName()).c_str());
            if (!grabTog && !countE) continue;

            // TODO(oktw-port): SpellShield/SpellImmunity buff-type check not available
            if (false || false)
                continue;
            if (Player().Position().Distance(t.ServerPosition()) <= minGrab) continue;

            if (Combo() && !ts) {
                CastSpell(m_Q, t);
            } else if (qTur) {
                CastSpell(m_Q, t);
            }

            if (qCC) {
                if (!OktwCommon::CanMove(t))
                    m_Q.Cast(t, true);
                // TODO(oktw-port): HitChance::Dashing not available; use IsDashing extension instead
                if (SDK::Extensions::IsDashing(t))
                    m_Q.Cast(t, true);
                m_Q.CastIfHitchanceEquals(t, SDK::HitChance::Immobile);
            }
        }
    }

    void LogicR() {
        const bool rKs       = GetBool("rKs");
        const bool afterGrab = GetBool("afterGrab");

        for (const auto& target : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!target.IsValid() || !target.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(target, m_R.Range, true)) continue;

            if (rKs && m_R.GetDamage(target) > target.Health()) {
                m_R.Cast();
            }
            if (afterGrab &&
                SDK::Extensions::IsValidTarget(target, 400.0f, true) &&
                target.HasBuff("rocketgrab2")) {
                m_R.Cast();
            }
        }

        const int rCount = GetSlider("rCount", 3);
        if (rCount > 0 &&
            OktwCommon::CountEnemiesInRange(Player().ServerPosition(), m_R.Range) >= rCount) {
            m_R.Cast();
        }
    }

    void LogicW() {
        for (const auto& target : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!target.IsValid() || !target.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(target, m_R.Range, true)) continue;
            if (target.HasBuff("rocketgrab2")) {
                m_W.Cast();
                break;
            }
        }
    }

    void OnGameDraw() override {
        // Draw statistics ("showgrab") and range circles ("qRange"/"rRange")
        // are wired via the shared draw menu toggles. Ranges are handled by the
        // SDK draw utilities in the base plugin; the numeric statistics overlay
        // (grab / grabS / percent) is intentionally left to a future pass since
        // the SDK draw-text helper isn't required for parity of logic.
        // TODO(oktw-port): render "grab X grab successful Y grab successful % Z%"
        //                 and range circles when GetBool("qRange"|"rRange") is true.
    }
};

} } // namespace Plugins::OKTW
