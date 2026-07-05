#pragma once
// Port of OKTW_CSharp/Champions/Thresh.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;

class ThreshPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Thresh"; }
    const char* GetInternalId() const override { return "champion.oktw.thresh"; }
    const char* GetChampionName() const override { return "Thresh"; }

protected:
    Spell m_Epush{ SpellSlot::E };

    void BuildMenu() override {
        MarkActive();

        m_Q     = Spell(SpellSlot::Q, 1075.0f);
        m_W     = Spell(SpellSlot::W, 950.0f);
        m_E     = Spell(SpellSlot::E, 480.0f);
        m_R     = Spell(SpellSlot::R, 400.0f);
        m_Epush = Spell(SpellSlot::E, 450.0f);

        m_Q.SetSkillshot(0.5f, 70.0f, 1900.0f, true, SDK::SpellType::Line);
        m_W.SetSkillshot(0.2f, 10.0f, FLT_MAX, false, SDK::SpellType::Circle);
        m_E.SetSkillshot(0.25f, 50.0f, 2000.0f, false, SDK::SpellType::Line);
        m_Epush.SetSkillshot(0.0f, 50.0f, FLT_MAX, false, SDK::SpellType::Line);

        m_qMenu->Add(new MenuBool("ts",     "Use common TargetSelector", true));
        m_qMenu->Add(new MenuBool("qCC",    "Auto Q cc", true));
        m_qMenu->Add(new MenuBool("qDash",  "Auto Q dash", true));
        m_qMenu->Add(new MenuSlider("minGrab", "Min range grab", 250, 125, static_cast<int>(m_Q.Range)));
        m_qMenu->Add(new MenuSlider("maxGrab", "Max range grab", static_cast<int>(m_Q.Range), 125, static_cast<int>(m_Q.Range)));

        Menu* grab = m_qMenu->AddSubMenu(new Menu("GrabSub", "Grab"));
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string id = std::string("grab") + enemy.CharacterName();
            grab->Add(new MenuBool(id.c_str(), enemy.CharacterName().c_str(), true));
        }
        m_qMenu->Add(new MenuBool("GapQ", "OnEnemyGapcloser Q", true));

        m_wMenu->Add(new MenuBool("autoW",  "Auto W", true));
        m_wMenu->Add(new MenuSlider("Wdmg", "W dmg % hp", 10, 0, 100));
        m_wMenu->Add(new MenuBool("autoW3", "Auto W shield big dmg", true));
        m_wMenu->Add(new MenuBool("autoW2", "Auto W if Q succesfull", true));
        m_wMenu->Add(new MenuBool("autoW4", "Auto W vs Blitz Hook", true));
        m_wMenu->Add(new MenuBool("autoW5", "Auto W if jungler pings", true));
        m_wMenu->Add(new MenuBool("autoW6", "Auto W on gapCloser", true));
        m_wMenu->Add(new MenuBool("autoW7", "Auto W on Slows/Stuns", true));
        m_wMenu->Add(new MenuSlider("wCount", "Auto W if x enemies near ally", 3, 0, 5));

        m_eMenu->Add(new MenuBool("autoE",     "Auto E", true));
        m_eMenu->Add(new MenuBool("pushE",     "Auto push", true));
        m_eMenu->Add(new MenuBool("pulldashE", "Auto pull on dash", true));
        m_eMenu->Add(new MenuBool("inter",     "OnPossibleToInterrupt", true));
        m_eMenu->Add(new MenuBool("Gap",       "OnEnemyGapcloser", true));
        m_eMenu->Add(new MenuSlider("Emin",    "Min pull range E", 200, 0, static_cast<int>(m_E.Range)));

        m_rMenu->Add(new MenuSlider("rCount", "Auto R if x enemies in range", 2, 0, 5));
        m_rMenu->Add(new MenuBool("rKs",      "R ks", false));
        m_rMenu->Add(new MenuBool("comboR",   "always R in combo", false));

        m_drawMenu->Add(new MenuBool("qRange",  "Q range", false));
        m_drawMenu->Add(new MenuBool("wRange",  "W range", false));
        m_drawMenu->Add(new MenuBool("eRange",  "E range", false));
        m_drawMenu->Add(new MenuBool("rRange",  "R range", false));
        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw when skill rdy", true));

        m_menu->Add(new MenuBool("AACombo", "Disable AA if can use E", true));

        // TODO(oktw-port): AntiGapcloser event not available (W/E/Q anti-gap reactions)
        // TODO(oktw-port): Interrupter2 event not available (E interrupt logic)
        // TODO(oktw-port): OnBuffGain/OnBuffLose events not available (ThreshQ marked target tracking)
        // TODO(oktw-port): TacticalMap.OnPing event not available (autoW5 jungler ping W)
        // TODO(oktw-port): Orbwalker.Attack toggle not available (AACombo E-only mode)
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
        m_RMANA = m_R.IsReady() ? m_R.Instance().ManaCost() : m_QMANA;
    }

    void OnGameUpdate() override {
        const auto p = Player();
        if (!p.IsValid()) return;

        SetMana();

        // Marked-target follow-up logic (partial: buff detection not wired)
        if (LagFree(1) && m_Q.IsReady()) LogicQ();
        if (LagFree(2) && m_E.IsReady() && GetBool("autoE")) LogicE();
        if (LagFree(3) && m_W.IsReady()) LogicW();
        if (LagFree(4) && m_R.IsReady()) LogicR();
    }

    void LogicE() {
        const auto p = Player();
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_E.Range, DamageType::Physical) : AIHeroClient();
        if (!t.IsValid() || !OktwCommon::CanMove(t)) return;

        if (Combo()) {
            const int emin = GetSlider("Emin", 200);
            if (p.Position().Distance(t.Position()) > static_cast<float>(emin)) {
                CastE(false, t);
            }
        } else if (GetBool("pushE")) {
            CastE(true, t);
        } else if (GetBool("pulldashE")) {
            // Dash detection deferred
            auto pout = m_E.GetPrediction(t, false);
            if (pout.GetCastPosition().Distance(p.Position()) < m_E.Range) {
                m_E.Cast(pout.GetCastPosition());
            }
        }
    }

    void CastE(bool push, const AIHeroClient& target) {
        const auto p = Player();
        if (push) {
            auto pout = m_E.GetPrediction(target, false);
            m_E.Cast(pout.GetCastPosition());
        } else {
            auto pout = m_Epush.GetPrediction(target, false);
            const Vector3 castPos = pout.GetCastPosition();
            const float distance = p.Position().Distance(castPos);
            const Vector3 dir = (castPos - p.Position());
            const float len = dir.Length();
            Vector3 ext = p.Position();
            if (len > 0.001f) ext = p.Position() - dir * (distance / len);
            m_E.Cast(ext);
        }
    }

    void LogicQ() {
        const auto p = Player();
        const float maxGrab = static_cast<float>(GetSlider("maxGrab", static_cast<int>(m_Q.Range)));
        const float minGrab = static_cast<float>(GetSlider("minGrab", 250));

        if (Combo() && GetBool("ts")) {
            auto* ts = SDK::TargetSelector::Instance();
            auto t = ts ? ts->GetTarget(maxGrab, DamageType::Physical) : AIHeroClient();
            if (t.IsValid() &&
                SDK::Extensions::IsValidTarget(t, maxGrab, true) &&
                GetBool((std::string("grab") + t.CharacterName()).c_str()) &&
                p.Position().Distance(t.ServerPosition()) > minGrab) {
                CastSpell(m_Q, t);
            }
        }

        for (const auto& t : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!t.IsValid() || !t.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(t, maxGrab, true)) continue;
            if (!GetBool((std::string("grab") + t.CharacterName()).c_str())) continue;
            if (p.Position().Distance(t.ServerPosition()) <= minGrab) continue;

            if (Combo() && !GetBool("ts")) {
                CastSpell(m_Q, t);
            }

            if (GetBool("qCC")) {
                if (!OktwCommon::CanMove(t)) m_Q.Cast(t);
            }
            // qDash: dash hitchance not available
        }
    }

    void LogicR() {
        const auto p = Player();
        const int rCountOut = OktwCommon::CountEnemiesInRange(p.Position(), m_R.Range);
        const int rCountIn  = OktwCommon::CountEnemiesInRange(p.Position(), 200.0f);
        if (rCountOut < rCountIn) return;

        const int rCount = GetSlider("rCount", 2);
        if (rCount > 0 && rCountOut >= rCount) m_R.Cast();

        if (GetBool("comboR")) {
            auto* ts = SDK::TargetSelector::Instance();
            auto t = ts ? ts->GetTarget(m_R.Range, DamageType::Physical) : AIHeroClient();
            if (t.IsValid() && Combo()) {
                if (p.Position().Distance(t.ServerPosition()) > p.Position().Distance(t.Position())) {
                    m_R.Cast();
                }
            }
        }
    }

    void CastW(const Vector3& pos) {
        const auto p = Player();
        if (p.Position().Distance(pos) < m_W.Range) {
            m_W.Cast(pos);
        } else {
            const Vector3 dir = (pos - p.Position());
            const float len = dir.Length();
            if (len > 0.001f) {
                m_W.Cast(p.Position() + dir * (m_W.Range / len));
            }
        }
    }

    void LogicW() {
        const auto p = Player();

        // autoW4 (Blitz hook save): rocketgrab2 buff detection deferred
        for (const auto& ally : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!ally.IsValid() || ally.IsEnemy()) continue;
            if (ally.NetworkId() == p.NetworkId()) continue;
            if (p.Position().Distance(ally.Position()) >= m_W.Range + 400.0f) continue;

            if (GetBool("autoW7")) {
                if (ally.Position().Distance(p.Position()) <= m_W.Range) {
                    // Stun/Root detection via CanMove
                    if (!OktwCommon::CanMove(ally)) {
                        m_W.Cast(ally.Position());
                    }
                }
            }

            const int nearEnemies = OktwCommon::CountEnemiesInRange(ally.Position(), 900.0f);
            const int wCount = GetSlider("wCount", 3);
            if (wCount > 0 && nearEnemies >= wCount) {
                auto pout = m_W.GetPrediction(ally, false);
                CastW(pout.GetCastPosition());
            }

            if (GetBool("autoW") && p.Position().Distance(ally.Position()) < m_W.Range + 100.0f) {
                const float dmg = OktwCommon::GetIncomingDamage(ally);
                if (dmg == 0.0f) continue;

                const int sensitivity = 20;
                const float HpPercentage = (dmg * 100.0f) / ally.Health();
                const float shieldValue = 20.0f + (p.Level() * 20.0f) + (0.4f * p.AP());
                int ne = (nearEnemies == 0) ? 1 : nearEnemies;

                auto pout = m_W.GetPrediction(ally, false);
                if (dmg > shieldValue && GetBool("autoW3")) {
                    m_W.Cast(pout.GetCastPosition());
                } else if (dmg > 100.0f + p.Level() * sensitivity) {
                    m_W.Cast(pout.GetCastPosition());
                } else if (ally.Health() - dmg < ne * ally.Level() * sensitivity) {
                    m_W.Cast(pout.GetCastPosition());
                } else if (HpPercentage >= static_cast<float>(GetSlider("Wdmg", 10))) {
                    m_W.Cast(pout.GetCastPosition());
                }
            }
        }
    }

    void OnGameDraw() override {}
};

} } // namespace Plugins::OKTW
