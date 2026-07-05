#pragma once
// Port of OKTW_CSharp/Champions/Twitch.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;
using SDK::UI::MenuKeyBind;

class TwitchPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Twitch"; }
    const char* GetInternalId() const override { return "champion.oktw.twitch"; }
    const char* GetChampionName() const override { return "Twitch"; }

protected:
    void BuildMenu() override {
        MarkActive();

        m_Q = Spell(SpellSlot::Q, 0.0f);
        m_W = Spell(SpellSlot::W, 950.0f);
        m_E = Spell(SpellSlot::E, 1200.0f);
        m_R = Spell(SpellSlot::R, 975.0f);

        m_W.SetSkillshot(0.25f, 100.0f, 1410.0f, false, SDK::SpellType::Circle);

        m_drawMenu->Add(new MenuBool("notif",   "Notification (timers)", true));
        m_drawMenu->Add(new MenuBool("eRange",  "E range", false));
        m_drawMenu->Add(new MenuBool("rRange",  "R range", false));
        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw only ready spells", true));

        m_qMenu->Add(new MenuSlider("countQ",     "Auto Q if x enemies are going in your direction 0-disable", 3, 0, 5));
        m_qMenu->Add(new MenuBool("autoQ",        "Auto Q in combo", true));
        m_qMenu->Add(new MenuBool("recallSafe",   "Safe Q recall", true));

        m_wMenu->Add(new MenuBool("autoW", "AutoW", true));

        m_eMenu->Add(new MenuBool("Eks",       "E ks", true));
        m_eMenu->Add(new MenuSlider("countE",  "Auto E if x stacks & out range AA", 6, 0, 6));
        m_eMenu->Add(new MenuBool("5e",        "Always E if 6 stacks", true));
        m_eMenu->Add(new MenuBool("jungleE",   "Jungle ks E", true));
        m_eMenu->Add(new MenuBool("Edead",     "Cast E before Twitch die", true));

        m_rMenu->Add(new MenuBool("Rks",      "R KS out range AA", true));
        m_rMenu->Add(new MenuSlider("countR", "Auto R if x enemies (combo)", 3, 0, 5));

        // TODO(oktw-port): Spellbook.OnCastSpell hook for safe Q recall
        // TODO(oktw-port): Obj_AI_Base.OnProcessSpellCast hook for "Edead" pre-death E cast
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
        m_RMANA = m_R.IsReady() ? m_R.Instance().ManaCost() : m_EMANA;
    }

    void OnGameUpdate() override {
        if (LagFree(0)) SetMana();
        if (LagFree(1) && m_E.IsReady()) LogicE();
        if (LagFree(2) && m_Q.IsReady()) LogicQ();
        if (LagFree(3) && GetBool("autoW") && m_W.IsReady()) LogicW();
        if (LagFree(4) && m_R.IsReady() && Combo()) LogicR();
    }

    void LogicR() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_R.Range, DamageType::Physical) : AIHeroClient();
        if (!t.IsValid()) return;

        const auto p = Player();
        const bool outsideAA = t.Position().Distance(p.Position()) > p.AttackRange() + p.BoundingRadius() + t.BoundingRadius();

        if (outsideAA && GetBool("Rks") && p.GetAutoAttackDamage(t, false) * 4.0f > t.Health()) {
            m_R.Cast();
        }

        const int rCount = GetSlider("countR", 3);
        if (rCount != 0 && OktwCommon::CountEnemiesInRange(t.Position(), 450.0f) >= rCount) {
            m_R.Cast();
        }
    }

    void LogicW() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_W.Range, DamageType::Physical) : AIHeroClient();
        if (!t.IsValid()) return;

        const auto p = Player();
        const bool outsideAA = t.Position().Distance(p.Position()) > p.AttackRange() + p.BoundingRadius() + t.BoundingRadius();

        if (Combo() && p.Mana() > m_WMANA + m_RMANA + m_EMANA &&
            (p.GetAutoAttackDamage(t, false) * 2.0f < t.Health() || outsideAA)) {
            CastSpell(m_W, t);
        } else if (!None() && p.Mana() > m_RMANA + m_WMANA + m_EMANA) {
            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (enemy.IsValid() && enemy.IsEnemy() &&
                    SDK::Extensions::IsValidTarget(enemy, m_W.Range, true) &&
                    !OktwCommon::CanMove(enemy)) {
                    m_W.Cast(enemy);
                }
            }
        }
    }

    void LogicQ() {
        const auto p = Player();
        auto* ts = SDK::TargetSelector::Instance();

        if (GetBool("autoQ") && Combo() && p.Mana() > m_RMANA + m_QMANA) {
            auto t = ts ? ts->GetTarget(p.AttackRange() + p.BoundingRadius() + 100.0f, DamageType::Physical) : AIHeroClient();
            if (t.IsValid()) m_Q.Cast();
        }

        const int cQ = GetSlider("countQ", 3);
        if (cQ == 0 || p.Mana() < m_RMANA + m_QMANA) return;

        int count = 0;
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(enemy, 3000.0f, true)) continue;
            // waypoints not available; fall back to distance to server position
            if (p.Position().Distance(enemy.ServerPosition()) < 600.0f) ++count;
        }
        if (count >= cQ) m_Q.Cast();
    }

    void LogicE() {
        const auto p = Player();
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(enemy, m_E.Range, true)) continue;
            if (!enemy.HasBuff("TwitchDeadlyVenom")) continue;
            if (!OktwCommon::ValidUlt(enemy)) continue;

            if (GetBool("Eks") && m_E.GetDamage(enemy) > enemy.Health()) {
                m_E.Cast();
            }

            if (p.Mana() > m_RMANA + m_EMANA) {
                const int buffsNum = OktwCommon::GetBuffCount(enemy, "TwitchDeadlyVenom");
                if (GetBool("5e") && buffsNum == 6) {
                    m_E.Cast();
                }
                const float buffTime = OktwCommon::GetPassiveTime(enemy, "TwitchDeadlyVenom");
                const bool outsideAA = p.Position().Distance(enemy.Position()) > p.AttackRange() + p.BoundingRadius() + enemy.BoundingRadius();
                const int cE = GetSlider("countE", 6);
                if (outsideAA && (p.ServerPosition().Distance(enemy.ServerPosition()) > 950.0f || buffTime < 1.0f) &&
                    cE > 0 && buffsNum >= cE) {
                    m_E.Cast();
                }
            }
        }
        JungleE();
    }

    void JungleE() {
        const auto p = Player();
        if (!GetBool("jungleE") || p.Mana() < m_RMANA + m_EMANA || p.Level() == 1) return;
        auto mobs = OktwCommon::GetMinions(p.ServerPosition(), m_E.Range, false, true);
        if (mobs.empty()) return;
        const auto& mob = mobs.front();
        if (m_E.GetDamage(mob) > mob.Health()) {
            m_E.Cast();
        }
    }

    void OnGameDraw() override {}
};

} } // namespace Plugins::OKTW
