#pragma once
// Port of OKTW_CSharp/Champions/Annie.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;

class AnniePlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Annie"; }
    const char* GetInternalId() const override { return "champion.oktw.annie"; }
    const char* GetChampionName() const override { return "Annie"; }

protected:
    Spell m_FR{ SpellSlot::R };  // flash-R longer range variant
    bool  m_haveStun = false;

    void BuildMenu() override {
        MarkActive();

        m_Q  = Spell(SpellSlot::Q, 625.0f);
        m_W  = Spell(SpellSlot::W, 550.0f);
        m_E  = Spell(SpellSlot::E);
        m_R  = Spell(SpellSlot::R, 625.0f);
        m_FR = Spell(SpellSlot::R, 1000.0f);
        m_Q.SetTargetted(0.25f, 1400.0f);
        m_W.SetSkillshot(0.30f, 80.0f, FLT_MAX, false, SDK::SpellType::Line);
        m_R.SetSkillshot(0.25f, 180.0f, FLT_MAX, false, SDK::SpellType::Circle);
        m_FR.SetSkillshot(0.25f, 180.0f, FLT_MAX, false, SDK::SpellType::Circle);

        m_drawMenu->Add(new MenuBool("qRange",  "Q range", false));
        m_drawMenu->Add(new MenuBool("wRange",  "W range", false));
        m_drawMenu->Add(new MenuBool("rRange",  "R range", false));
        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw only ready spells", true));

        m_qMenu->Add(new MenuBool("autoQ",   "Auto Q", true));
        m_qMenu->Add(new MenuBool("harassQ", "Harass Q", true));

        m_wMenu->Add(new MenuBool("autoW",   "Auto W", true));
        m_wMenu->Add(new MenuBool("harassW", "Harass W", true));

        m_eMenu->Add(new MenuBool("autoE", "Auto E stack stun", true));

        Menu* um = m_rMenu->AddSubMenu(new Menu("UMSub", "Ultimate Manager"));
        static const char* rModes[] = { "Normal", "Always", "Never", "Always Stun" };
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string id = std::string("UM") + enemy.CharacterName();
            um->Add(new MenuList(id.c_str(), enemy.CharacterName().c_str(), rModes, 4, 0));
        }
        m_rMenu->Add(new MenuBool("autoRks",    "Auto R KS", true));
        m_rMenu->Add(new MenuBool("autoRcombo", "Auto R Combo if stun is ready", true));
        m_rMenu->Add(new MenuSlider("rCount",   "Auto R x enemies", 3, 2, 5));
        m_rMenu->Add(new MenuBool("tibers",     "Tibbers Auto Pilot", true));
        m_rMenu->Add(new MenuSlider("rCountFlash", "Auto flash + R stun x enemies", 4, 2, 5));

        m_farmMenu->Add(new MenuBool("farmQ", "Farm Q", true));
        m_farmMenu->Add(new MenuBool("farmW", "Lane clear W", false));
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
        m_RMANA = (!m_R.IsReady() || HaveTibers()) ? 0.0f : m_R.Instance().ManaCost();
    }

    bool HaveTibers() const { return Player().HasBuff("infernalguardiantimer"); }

    void OnGameUpdate() override {
        const auto p = Player();
        if (!p.IsValid() || p.HasBuff("Recall")) return;
        m_haveStun = p.HasBuff("pyromania_particle");
        SetMana();

        if (m_R.IsReady() && (LagFree(1) || LagFree(3)) && !HaveTibers())
            LogicR();

        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_Q.Range, DamageType::Magical) : AIHeroClient();
        if (t.IsValid() && LagFree(2)) {
            if (m_Q.IsReady() && GetBool("autoQ")) LogicQ(t);
            if (m_W.IsReady() && GetBool("autoW") &&
                SDK::Extensions::IsValidTarget(t, m_W.Range, true))
                LogicW(t);
        } else if (m_Q.IsReady() || m_W.IsReady()) {
            if (GetBool("farmQ")) {
                const bool supportMode = Shared().supportMode && Shared().supportMode->Value;
                if (supportMode) {
                    if (LaneClear() && p.Mana() > m_RMANA + m_QMANA) Farm();
                } else {
                    if ((!m_haveStun || LaneClear()) && Harass()) Farm();
                }
            }
        }

        if (LagFree(3) && !m_haveStun) {
            if (m_E.IsReady() && !LaneClear() && GetBool("autoE") &&
                p.Mana() > m_RMANA + m_EMANA + m_QMANA + m_WMANA)
                m_E.Cast();
        }
    }

    void LogicR() {
        const auto p = Player();
        const bool hasFlash = false;  // flash detection deferred
        const float realRange = hasFlash ? m_FR.Range : m_R.Range;
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(enemy, realRange, true)) continue;
            if (!OktwCommon::ValidUlt(enemy)) continue;

            if (!SDK::Extensions::IsValidTarget(enemy, m_R.Range, true)) continue;

            const std::string umId = std::string("UM") + enemy.CharacterName();
            const int rMode = GetList(umId.c_str(), 0);
            if (rMode == 2) continue;

            const auto pout = m_R.GetPrediction(enemy, true);
            const int aoe = pout.AoeTargetsHitCount;

            if (rMode == 1) { m_R.Cast(pout.GetCastPosition()); continue; }
            if (rMode == 3 && m_haveStun) { m_R.Cast(pout.GetCastPosition()); continue; }

            const int rCount = GetSlider("rCount", 3);
            if (rCount > 0 && aoe >= rCount) {
                m_R.Cast(pout.GetCastPosition());
            } else if (Combo() && m_haveStun && GetBool("autoRcombo")) {
                m_R.Cast(pout.GetCastPosition());
            } else if (GetBool("autoRks")) {
                float combo = OktwCommon::GetKsDamage(enemy, m_R);
                if (m_W.IsReady() && (m_RMANA + m_WMANA) < p.Mana()) combo += m_W.GetDamage(enemy);
                if (m_Q.IsReady() && (m_RMANA + m_WMANA + m_QMANA) < p.Mana()) combo += m_Q.GetDamage(enemy);
                if (enemy.Health() < combo) m_R.Cast(pout.GetCastPosition());
            }
        }
    }

    void LogicQ(const AIHeroClient& t) {
        const auto p = Player();
        if (Combo() && (m_RMANA + m_WMANA) < p.Mana()) {
            m_Q.Cast(t);
        } else if (Harass() && (m_RMANA + m_WMANA + m_QMANA) < p.Mana() &&
                   GetBool("harassQ") &&
                   GetBool((std::string("Harass") + t.CharacterName()).c_str())) {
            m_Q.Cast(t);
        } else {
            const float qDmg = OktwCommon::GetKsDamage(t, m_Q);
            const float wDmg = m_W.GetDamage(t);
            if (qDmg > t.Health()) m_Q.Cast(t);
            else if (qDmg + wDmg > t.Health() && p.Mana() > m_QMANA + m_WMANA) m_Q.Cast(t);
        }
    }

    void LogicW(const AIHeroClient& t) {
        const auto p = Player();
        const auto pout = m_W.GetPrediction(t, true);
        if (Combo() && (m_RMANA + m_WMANA) < p.Mana()) {
            m_W.Cast(pout.GetCastPosition());
        } else if (Harass() && (m_RMANA + m_WMANA + m_QMANA) < p.Mana() && GetBool("harassW")) {
            m_W.Cast(pout.GetCastPosition());
        } else {
            const float wDmg = OktwCommon::GetKsDamage(t, m_W);
            const float qDmg = m_Q.GetDamage(t);
            if (wDmg > t.Health()) m_W.Cast(pout.GetCastPosition());
            else if (qDmg + wDmg > t.Health() && p.Mana() > m_QMANA + m_WMANA)
                m_W.Cast(pout.GetCastPosition());
        }
    }

    void Farm() {
        const auto p = Player();
        if (LaneClear()) {
            auto mobs = OktwCommon::GetMinions(p.ServerPosition(), m_Q.Range, false, true);
            if (!mobs.empty()) {
                const auto& mob = mobs.front();
                if (m_W.IsReady()) { m_W.Cast(mob.Position()); return; }
                if (m_Q.IsReady()) { m_Q.Cast(mob); return; }
            }
        }
        auto minions = OktwCommon::GetMinions(p.ServerPosition(), m_Q.Range);
        if (m_Q.IsReady()) {
            for (const auto& mn : minions) {
                if (mn.Health() < m_Q.GetDamage(mn) &&
                    mn.Health() > p.GetAutoAttackDamage(mn, false)) {
                    m_Q.Cast(mn);
                    break;
                }
            }
        }
    }

    void OnGameDraw() override {}
};

} } // namespace Plugins::OKTW
