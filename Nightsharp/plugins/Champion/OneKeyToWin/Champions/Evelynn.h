#pragma once
// Port of OKTW_CSharp/Champions/Evelynn.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;
using SDK::UI::MenuKeyBind;

class EvelynnPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Evelynn"; }
    const char* GetInternalId() const override { return "champion.oktw.evelynn"; }
    const char* GetChampionName() const override { return "Evelynn"; }

protected:
    void BuildMenu() override {
        MarkActive();

        m_Q = Spell(SpellSlot::Q, 500.0f);
        m_W = Spell(SpellSlot::W, 700.0f);
        m_E = Spell(SpellSlot::E, 250.0f);
        m_R = Spell(SpellSlot::R, 650.0f);

        m_R.SetSkillshot(0.25f, 300.0f, FLT_MAX, false, SDK::SpellType::Circle);

        // Draw
        m_drawMenu->Add(new MenuBool("qRange",  "Q range", false));
        m_drawMenu->Add(new MenuBool("wRange",  "W range", false));
        m_drawMenu->Add(new MenuBool("eRange",  "E range", false));
        m_drawMenu->Add(new MenuBool("rRange",  "R range", false));
        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw only ready spells", true));

        // Q
        m_qMenu->Add(new MenuBool("autoQ", "Auto Q", true));

        // W
        m_wMenu->Add(new MenuBool("autoW", "Auto W", true));
        m_wMenu->Add(new MenuBool("slowW", "Auto W slow", true));

        // E
        m_eMenu->Add(new MenuBool("autoE", "Auto E", true));

        // R
        m_rMenu->Add(new MenuSlider("rCount", "Auto R x enemies", 3, 0, 5));
        m_rMenu->Add(new MenuKeyBind("useR", "Semi-manual cast R key", 'T', SDK::KeyBindType::Press));

        // Farm
        m_farmMenu->Add(new MenuBool("jungleQ", "Jungle Q", true));
        m_farmMenu->Add(new MenuBool("jungleE", "Jungle E", true));
        m_farmMenu->Add(new MenuBool("laneQ",   "Lane clear Q", true));
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
        SetMana();

        // Semi-manual R (keybind) — C# uses TargetSelector.DamageType.Physical
        if (GetKey("useR")) {
            auto* ts = SDK::TargetSelector::Instance();
            auto t = ts ? ts->GetTarget(m_R.Range, SDK::DamageType::Physical) : AIHeroClient();
            if (t.IsValid()) {
                // TODO(oktw-port): Spell::CastIfWillHit(target, minHits, aoe) not exposed on SDK::Spell;
                // approximate by prediction AoE check, then unconditional cast.
                const auto pout = m_R.GetPrediction(t, true);
                if (pout.AoeTargetsHitCount >= 2) {
                    m_R.Cast(pout.GetCastPosition());
                }
                m_R.Cast(t);
            }
        }

        if (Combo()) {
            if (LagFree(1) && m_Q.IsReady() && GetBool("autoQ")) LogicQ();
            if (LagFree(2) && m_E.IsReady() && GetBool("autoE")) LogicE();
            if (LagFree(3) && m_W.IsReady())                     LogicW();
            if (LagFree(4) && m_R.IsReady())                     LogicR();
        } else if (LaneClear()) {
            Jungle();
        }
    }

    void LogicQ() {
        // C#: if (Player.CountEnemiesInRange(Q.Range) > 0) Q.Cast();
        if (OktwCommon::CountEnemiesInRange(Player().ServerPosition(), m_Q.Range) > 0) {
            m_Q.Cast();
        }
    }

    void LogicE() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_E.Range, SDK::DamageType::Physical) : AIHeroClient();
        if (t.IsValid()) {
            m_E.Cast(t);
        }
    }

    void LogicW() {
        const auto p = Player();
        const float mana = p.Mana();
        if (GetBool("autoW") && mana > m_RMANA + m_EMANA + m_QMANA &&
            OktwCommon::CountEnemiesInRange(p.ServerPosition(), m_W.Range) > 0) {
            m_W.Cast();
        } else if (GetBool("slowW") && mana > m_RMANA + m_EMANA + m_QMANA &&
                   false /* TODO(oktw-port): BuffType::Slow unavailable in SDK */) {
            m_W.Cast();
        }
    }

    void LogicR() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_R.Range, SDK::DamageType::Physical) : AIHeroClient();
        if (!t.IsValid()) return;

        const auto pout = m_R.GetPrediction(t, true);
        int aoeCount = pout.AoeTargetsHitCount;
        if (aoeCount == 0) aoeCount = 1;

        const int rCount = GetSlider("rCount", 3);
        if (rCount > 0 && rCount <= aoeCount) {
            m_R.Cast(pout.GetCastPosition());
        }

        const auto p = Player();
        if (p.HealthPercent() < 60.0f) {
            const float dmg = OktwCommon::GetIncomingDamage(p);
            const int enemys = OktwCommon::CountEnemiesInRange(p.ServerPosition(), 700.0f);
            if (p.Health() - dmg < static_cast<float>(enemys * p.Level() * 20)) {
                m_R.Cast(pout.GetCastPosition());
            } else if (p.Health() - dmg < static_cast<float>(p.Level() * 10)) {
                m_R.Cast(pout.GetCastPosition());
            }
        }
    }

    void Jungle() {
        const auto p = Player();
        const float manaPct = p.MaxMana() > 0.0f ? (p.Mana() * 100.0f / p.MaxMana()) : 0.0f;
        if (manaPct < static_cast<float>(m_manaSlider ? m_manaSlider->Value : 50)) return;

        // C#: Cache.GetMinions(Player.ServerPosition, Q.Range, MinionTeam.Neutral)
        auto mobs = OktwCommon::GetMinions(p.ServerPosition(), m_Q.Range, false, true);
        if (!mobs.empty()) {
            const auto& mob = mobs.front();
            if (GetBool("jungleE") && m_E.IsReady()) {
                m_E.Cast(mob);
            }
            if (GetBool("jungleQ") && m_Q.IsReady()) {
                m_Q.Cast();
            }
        }

        if (GetBool("laneQ") && m_Q.IsReady()) {
            m_Q.Cast();
        }
    }

    void OnGameDraw() override {
        // TODO(oktw-port): range drawing relies on SDK draw utilities; the shared
        // draw menu items (qRange/wRange/eRange/rRange/onlyRdy) are registered so
        // the UI matches C#. Actual circle rendering is deferred to SDK helpers.
    }
};

} } // namespace Plugins::OKTW
