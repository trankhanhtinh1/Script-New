#pragma once
// Port of OKTW_CSharp/Champions/Velkoz.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;
using SDK::UI::MenuKeyBind;

class VelkozPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Velkoz"; }
    const char* GetInternalId() const override { return "champion.oktw.velkoz"; }
    const char* GetChampionName() const override { return "Velkoz"; }

protected:
    Spell m_QSplit{ SpellSlot::Q };
    Spell m_QDummy{ SpellSlot::Q };

    void BuildMenu() override {
        MarkActive();

        m_Q      = Spell(SpellSlot::Q, 1180.0f);
        m_QSplit = Spell(SpellSlot::Q, 1000.0f);
        // QDummy range = sqrt(Q.Range^2 + QSplit.Range^2)
        m_QDummy = Spell(SpellSlot::Q, std::sqrt(1180.0f * 1180.0f + 1000.0f * 1000.0f));
        m_W = Spell(SpellSlot::W, 1000.0f);
        m_E = Spell(SpellSlot::E, 800.0f);
        m_R = Spell(SpellSlot::R, 1500.0f);

        m_Q.SetSkillshot(0.25f, 70.0f, 1300.0f, true,  SDK::SpellType::Line);
        m_QSplit.SetSkillshot(0.1f, 70.0f, 2100.0f, true, SDK::SpellType::Line);
        m_QDummy.SetSkillshot(0.5f, 55.0f, 1200.0f, false, SDK::SpellType::Line);
        m_W.SetSkillshot(0.25f, 85.0f, 1700.0f, false, SDK::SpellType::Line);
        m_E.SetSkillshot(1.0f,  180.0f, FLT_MAX, false, SDK::SpellType::Circle);
        m_R.SetSkillshot(0.1f,  80.0f,  FLT_MAX, false, SDK::SpellType::Line);

        m_drawMenu->Add(new MenuBool("qRange",  "Q range", false));
        m_drawMenu->Add(new MenuBool("wRange",  "W range", false));
        m_drawMenu->Add(new MenuBool("eRange",  "E range", false));
        m_drawMenu->Add(new MenuBool("rRange",  "R range", false));
        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw when skill rdy", true));

        m_qMenu->Add(new MenuBool("autoQ",       "Auto Q", true));
        m_qMenu->Add(new MenuBool("harassQ",     "Harass Q", true));
        m_qMenu->Add(new MenuSlider("QHarassMana", "Harass Mana", 30, 0, 100));

        m_wMenu->Add(new MenuBool("autoW",   "Auto W", true));
        m_wMenu->Add(new MenuBool("harassW", "Harass W", true));

        m_eMenu->Add(new MenuBool("autoE",         "Auto E", true));
        m_eMenu->Add(new MenuBool("harassE",       "Harass E", false));
        m_eMenu->Add(new MenuBool("EInterrupter",  "Auto E Interrupter", true));

        Menu* egc = m_eMenu->AddSubMenu(new Menu("EGCSub", "E Gap Closer"));
        static const char* gcModes[] = { "Dash end position", "Player position", "Prediction" };
        egc->Add(new MenuList("EmodeGC", "Gap Closer position mode", gcModes, 3, 0));
        Menu* egcOn = egc->AddSubMenu(new Menu("EGCOnSub", "Cast on enemy:"));
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string id = std::string("EGCchampion") + enemy.CharacterName();
            egcOn->Add(new MenuBool(id.c_str(), enemy.CharacterName().c_str(), true));
        }

        m_rMenu->Add(new MenuBool("autoR", "Auto R KS", true));

        m_farmMenu->Add(new MenuBool("farmE",   "Lane clear E", true));
        m_farmMenu->Add(new MenuBool("farmW",   "Lane clear W", true));
        m_farmMenu->Add(new MenuBool("jungleQ", "Jungle clear Q", true));
        m_farmMenu->Add(new MenuBool("jungleW", "Jungle clear W", true));
        m_farmMenu->Add(new MenuBool("jungleE", "Jungle clear E", true));

        // TODO(oktw-port): Interrupter2, AntiGapcloser, GameObject.OnCreate events not available
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
        if (m_R.IsReady() && GetBool("autoR")) LogicR();

        if (LagFree(0)) { SetMana(); Jungle(); }

        if (m_Q.IsReady() && GetBool("autoQ")) LogicQ();

        if (LagFree(3) && m_E.IsReady() && GetBool("autoE")) LogicE();

        if (LagFree(4)) {
            if (m_W.IsReady() && GetBool("autoW")) LogicW();
        }
    }

    void LogicR() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_R.Range, DamageType::Magical) : AIHeroClient();
        const auto p = Player();
        if (t.IsValid() && OktwCommon::CountEnemiesInRange(p.Position(), 400.0f) == 0) {
            float rDmg = OktwCommon::GetKsDamage(t, m_R);
            const float distance = p.Position().Distance(t.Position());

            if (distance > 900.0f && OktwCommon::CanMove(t)) {
                const float adjust = (m_R.Range - distance) / 600.0f;
                rDmg = rDmg * adjust;
            }

            if (rDmg > t.Health() && OktwCommon::ValidUlt(t)) {
                m_R.Cast(t);
            }
        }
    }

    void LogicE() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_E.Range, DamageType::Magical) : AIHeroClient();
        const auto p = Player();
        if (t.IsValid()) {
            if (Combo() && p.Mana() > m_RMANA + m_EMANA) {
                CastSpell(m_E, t);
            } else if (Harass() && OktwCommon::CanHarras() && GetBool("harassE") &&
                       GetBool((std::string("Harass") + t.CharacterName()).c_str()) &&
                       p.Mana() > m_RMANA + m_EMANA + m_WMANA + m_EMANA) {
                CastSpell(m_E, t);
            } else {
                const float eDmg = OktwCommon::GetKsDamage(t, m_E);
                const float qDmg = m_Q.GetDamage(t);
                if (qDmg + eDmg > t.Health()) {
                    if (eDmg > t.Health()) {
                        CastSpell(m_E, t);
                    } else if (p.Mana() > m_QMANA + m_EMANA) {
                        CastSpell(m_E, t);
                    }
                    return;
                }
            }
            if (!None() && p.Mana() > m_RMANA + m_EMANA) {
                for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                    if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                    if (SDK::Extensions::IsValidTarget(enemy, m_E.Range, true) &&
                        !OktwCommon::CanMove(enemy)) {
                        m_E.Cast(enemy);
                    }
                }
            }
        } else if (FarmSpells() && GetBool("farmE")) {
            auto minions = OktwCommon::GetMinions(Player().ServerPosition(), m_E.Range);
            std::vector<SDK::AIBaseClient> baseList(minions.begin(), minions.end());
            auto farm = m_E.GetCircularFarmLocation(baseList, m_E.Width);
            if (farm.MinionsHit >= FarmMinions()) m_E.Cast(farm.Position);
        }
    }

    void LogicW() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_W.Range, DamageType::Magical) : AIHeroClient();
        const auto p = Player();
        if (t.IsValid()) {
            if (Combo() && p.Mana() > m_RMANA + m_WMANA) {
                CastSpell(m_W, t);
            } else if (Harass() && GetBool("harassW") &&
                       GetBool((std::string("Harass") + t.CharacterName()).c_str()) &&
                       p.Mana() > m_RMANA + m_WMANA + m_EMANA + m_QMANA + m_WMANA &&
                       OktwCommon::CanHarras()) {
                CastSpell(m_W, t);
            } else {
                const float wDmg = OktwCommon::GetKsDamage(t, m_W);
                const float qDmg = m_Q.GetDamage(t);
                if (wDmg > t.Health()) {
                    CastSpell(m_W, t);
                } else if (qDmg + wDmg > t.Health() && p.Mana() > m_QMANA + m_WMANA) {
                    CastSpell(m_W, t);
                }
            }
            if (!None() && p.Mana() > m_RMANA + m_WMANA) {
                for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                    if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                    if (SDK::Extensions::IsValidTarget(enemy, m_W.Range, true) &&
                        !OktwCommon::CanMove(enemy)) {
                        m_W.Cast(enemy);
                    }
                }
            }
        } else if (FarmSpells() && GetBool("farmW")) {
            auto minions = OktwCommon::GetMinions(Player().ServerPosition(), m_W.Range);
            std::vector<SDK::AIBaseClient> baseList(minions.begin(), minions.end());
            auto farm = m_W.GetLineFarmLocation(baseList, m_W.Width);
            if (farm.MinionsHit >= FarmMinions()) m_W.Cast(farm.Position);
        }
    }

    void LogicQ() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_QDummy.Range, DamageType::Magical) : AIHeroClient();
        const auto p = Player();
        if (!t.IsValid()) return;

        if (LagFree(1) || LagFree(2)) {
            if (Combo() && p.Mana() > m_RMANA + m_QMANA) {
                CastQ(t);
            } else if (Harass() && OktwCommon::CanHarras() && GetBool("harassQ") &&
                       GetBool((std::string("Harass") + t.CharacterName()).c_str()) &&
                       p.ManaPercent() > GetSlider("QHarassMana", 30)) {
                CastQ(t);
            } else {
                const float qDmg = OktwCommon::GetKsDamage(t, m_Q);
                const float wDmg = m_W.GetDamage(t);
                if (qDmg > t.Health()) CastQ(t);
                else if (qDmg + wDmg > t.Health() && p.Mana() > m_QMANA + m_WMANA)
                    CastQ(t);
            }
            if (!None() && p.Mana() > m_RMANA + m_QMANA) {
                for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                    if (enemy.IsValid() && enemy.IsEnemy() &&
                        SDK::Extensions::IsValidTarget(enemy, m_QDummy.Range, true) &&
                        !OktwCommon::CanMove(enemy)) {
                        CastQ(t);
                    }
                }
            }
        }
    }

    void CastQ(const SDK::AIBaseClient& t) {
        auto pred = m_Q.GetPrediction(t, true);
        if (pred.Hitchance >= SDK::HitChance::High) {
            CastSpell(m_Q, t);
        } else {
            // TODO(oktw-port): dummy angled Q with AimQ/BestAim requires collision APIs
            auto dummyPred = m_QDummy.GetPrediction(t, true);
            if (dummyPred.Hitchance >= SDK::HitChance::High) {
                CastSpell(m_Q, t);
            }
        }
    }

    void Jungle() {
        if (!LaneClear()) return;
        const auto p = Player();
        if (p.Mana() <= m_RMANA + m_QMANA) return;
        auto mobs = OktwCommon::GetMinions(p.ServerPosition(), m_E.Range, false, true);
        if (mobs.empty()) return;
        const auto& mob = mobs.front();
        if (m_W.IsReady() && GetBool("jungleW")) { m_W.Cast(mob.ServerPosition()); return; }
        if (m_Q.IsReady() && GetBool("jungleQ")) { m_Q.Cast(mob.ServerPosition()); return; }
        if (m_E.IsReady() && GetBool("jungleE")) { m_E.Cast(mob.ServerPosition()); return; }
    }

    void OnGameDraw() override {}
};

} } // namespace Plugins::OKTW
