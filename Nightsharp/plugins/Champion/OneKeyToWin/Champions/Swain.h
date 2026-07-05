#pragma once
// Port of OKTW_CSharp/Champions/Swain.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;

class SwainPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Swain"; }
    const char* GetInternalId() const override { return "champion.oktw.swain"; }
    const char* GetChampionName() const override { return "Swain"; }

protected:
    bool m_Ractive = false;

    void BuildMenu() override {
        MarkActive();

        m_Q = Spell(SpellSlot::Q, 700.0f);
        m_W = Spell(SpellSlot::W, 900.0f);
        m_E = Spell(SpellSlot::E, 625.0f);
        m_R = Spell(SpellSlot::R, 675.0f);
        m_Q.SetSkillshot(0.5f, 200.0f, FLT_MAX, false, SDK::SpellType::Circle);
        m_W.SetSkillshot(1.5f, 240.0f, FLT_MAX, false, SDK::SpellType::Circle);

        m_drawMenu->Add(new MenuBool("qRange",  "Q range", false));
        m_drawMenu->Add(new MenuBool("wRange",  "W range", false));
        m_drawMenu->Add(new MenuBool("eRange",  "E range", false));
        m_drawMenu->Add(new MenuBool("rRange",  "R range", false));
        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw only ready spells", true));

        m_qMenu->Add(new MenuBool("autoQ",   "Auto Q", true));
        m_qMenu->Add(new MenuBool("harassQ", "Harass Q", true));
        Menu* quseOn = m_qMenu->AddSubMenu(new Menu("QuseOn", "Use on:"));
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string id = std::string("Quse") + enemy.CharacterName();
            quseOn->Add(new MenuBool(id.c_str(), enemy.CharacterName().c_str(), true));
        }

        m_wMenu->Add(new MenuBool("autoW",  "Auto W on hard CC", true));
        m_wMenu->Add(new MenuBool("Wspell", "W on special spell detection", true));
        m_wMenu->Add(new MenuBool("Int",    "W On Interruptable Target", true));
        static const char* wCombo[] = { "always", "run - cheese" };
        m_wMenu->Add(new MenuList("WmodeCombo", "W combo mode", wCombo, 2, 1));
        m_wMenu->Add(new MenuSlider("Waoe", "Auto W x enemies", 3, 0, 5));
        Menu* wgap = m_wMenu->AddSubMenu(new Menu("WGap", "W Gap Closer"));
        static const char* wGCMode[] = { "Dash end position", "My hero position" };
        wgap->Add(new MenuList("WmodeGC", "Gap Closer position mode", wGCMode, 2, 0));
        Menu* wgapEnemies = wgap->AddSubMenu(new Menu("WGapOn", "Cast on enemy:"));
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string id = std::string("WGCchampion") + enemy.CharacterName();
            wgapEnemies->Add(new MenuBool(id.c_str(), enemy.CharacterName().c_str(), true));
        }

        m_eMenu->Add(new MenuBool("autoE",   "Auto E", true));
        m_eMenu->Add(new MenuBool("harassE", "Harass E", true));
        Menu* euseOn = m_eMenu->AddSubMenu(new Menu("EuseOn", "Use on:"));
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string id = std::string("Euse") + enemy.CharacterName();
            euseOn->Add(new MenuBool(id.c_str(), enemy.CharacterName().c_str(), true));
        }

        m_rMenu->Add(new MenuBool("autoR",   "Auto R", true));
        m_rMenu->Add(new MenuBool("harassR", "Harass R", true));
        m_rMenu->Add(new MenuSlider("Raoe", "Auto R if x enemies in range", 2, 1, 5));

        m_farmMenu->Add(new MenuBool("farmW",    "Lane clear W", true));
        m_farmMenu->Add(new MenuBool("farmR",    "Lane clear R", true));
        m_farmMenu->Add(new MenuBool("jungleE",  "Jungle clear E", true));
        m_farmMenu->Add(new MenuBool("jungleQ",  "Jungle clear Q", true));
        m_farmMenu->Add(new MenuBool("jungleW",  "Jungle clear W", true));
        m_farmMenu->Add(new MenuBool("jungleR",  "Jungle clear R", true));

        // TODO(oktw-port): AntiGapcloser.OnEnemyGapcloser, Interrupter2.OnInterruptableTarget,
        //                  Obj_AI_Base.OnProcessSpellCast not available in NightSharp SDK.
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
        m_RMANA = m_R.IsReady() ? m_R.Instance().ManaCost() : m_WMANA;
    }

    void OnGameUpdate() override {
        if (LagFree(0)) {
            SetMana();
            m_Ractive = Player().HasBuff("SwainMetamorphism");
            Jungle();
        }
        if (LagFree(1) && m_E.IsReady() && GetBool("autoE")) LogicE();
        if (LagFree(2) && m_Q.IsReady() && GetBool("autoQ")) LogicQ();
        if (LagFree(3) && m_W.IsReady()) LogicW();
        if (LagFree(4) && m_R.IsReady() && GetBool("autoR")) LogicR();
    }

    void LogicR() {
        const auto p = Player();
        if (m_Ractive) {
            if (FarmSpells() && GetBool("farmR")) {
                auto allMinions = OktwCommon::GetMinions(p.Position(), m_R.Range);
                auto mobs = OktwCommon::GetMinions(p.Position(), m_R.Range, false, true);
                if (!mobs.empty()) {
                    if (!GetBool("jungleR")) m_R.Cast();
                } else if (!allMinions.empty()) {
                    if (static_cast<int>(allMinions.size()) < 2) m_R.Cast();
                } else {
                    m_R.Cast();
                }
            } else if ((OktwCommon::CountEnemiesInRange(p.Position(), m_R.Range + 400.0f) == 0 ||
                        p.Mana() < m_EMANA) &&
                       ((Harass() && GetBool("farmR")) || None())) {
                m_R.Cast();
            }
        } else {
            const int countAOE = OktwCommon::CountEnemiesInRange(p.Position(), m_R.Range);
            if (countAOE > 0) {
                if (Combo() && GetBool("autoR")) m_R.Cast();
                else if (Harass() && GetBool("harassR")) m_R.Cast();
                else if (countAOE >= GetSlider("Raoe", 2)) m_R.Cast();
            }
            if (FarmSpells() && GetBool("farmR")) {
                auto allMinions = OktwCommon::GetMinions(p.ServerPosition(), m_R.Range);
                if (static_cast<int>(allMinions.size()) >= FarmMinions()) m_R.Cast();
            }
        }
    }

    void LogicW() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_W.Range, DamageType::Magical) : AIHeroClient();
        const auto p = Player();
        if (t.IsValid()) {
            if (Combo()) {
                if (GetList("WmodeCombo", 1) == 1) {
                    const auto pout = m_W.GetPrediction(t);
                    if (pout.GetCastPosition().Distance(t.Position()) > 100.0f) {
                        if (p.Position().Distance(t.ServerPosition()) > p.Position().Distance(t.Position())) {
                            if (t.Position().Distance(p.ServerPosition()) < t.Position().Distance(p.Position()))
                                CastSpell(m_W, t);
                        } else {
                            if (t.Position().Distance(p.ServerPosition()) > t.Position().Distance(p.Position()))
                                CastSpell(m_W, t);
                        }
                    }
                } else {
                    CastSpell(m_W, t);
                }
            }

            const int waoe = GetSlider("Waoe", 3);
            if (waoe > 0) {
                auto pred = m_W.GetPrediction(t, true);
                if (pred.AoeTargetsHitCount >= waoe) m_W.Cast(pred.GetCastPosition());
            }
        } else if (FarmSpells() && GetBool("farmW")) {
            const auto minions = OktwCommon::GetMinions(p.ServerPosition(), m_W.Range);
            std::vector<SDK::AIBaseClient> baseList(minions.begin(), minions.end());
            auto farm = m_W.GetCircularFarmLocation(baseList, m_W.Width);
            if (farm.MinionsHit >= FarmMinions()) m_W.Cast(farm.Position);
        }

        if (GetBool("autoW")) {
            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                if (!SDK::Extensions::IsValidTarget(enemy, m_W.Range, true)) continue;
                if (!OktwCommon::CanMove(enemy)) m_W.Cast(enemy);
            }
        }
    }

    void LogicQ() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_Q.Range, DamageType::Magical) : AIHeroClient();
        const auto p = Player();
        if (t.IsValid()) {
            if (t.Health() < OktwCommon::GetKsDamage(t, m_Q) + m_E.GetDamage(t)) {
                m_Q.Cast(t);
            }
            const std::string qid = std::string("Quse") + t.CharacterName();
            if (!GetBool(qid.c_str())) return;
            if (Combo() && p.Mana() > m_RMANA + m_EMANA) {
                m_Q.Cast(t);
            } else if (Harass() && GetBool("harassQ") &&
                       p.Mana() > m_RMANA + m_EMANA + m_WMANA + m_EMANA &&
                       GetBool((std::string("Harass") + t.CharacterName()).c_str())) {
                m_Q.Cast(t);
            } else if (Combo() || Harass()) {
                for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                    if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                    if (!SDK::Extensions::IsValidTarget(enemy, m_Q.Range, true)) continue;
                    if (!OktwCommon::CanMove(enemy)) m_Q.Cast(enemy);
                }
            }
        }
    }

    void LogicE() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_E.Range, DamageType::Magical) : AIHeroClient();
        const auto p = Player();
        if (t.IsValid()) {
            if (t.Health() < m_E.GetDamage(t) + OktwCommon::GetKsDamage(t, m_Q)) {
                m_E.Cast(t);
            }
            const std::string eid = std::string("Euse") + t.CharacterName();
            if (!GetBool(eid.c_str())) return;
            if (Combo() && p.Mana() > m_RMANA + m_EMANA) {
                m_E.Cast(t);
            } else if (Harass() && GetBool("harassE") &&
                       p.Mana() > m_RMANA + m_EMANA + m_WMANA + m_EMANA &&
                       GetBool((std::string("Harass") + t.CharacterName()).c_str())) {
                m_E.Cast(t);
            }
        }
    }

    void Jungle() {
        if (!LaneClear()) return;
        const auto p = Player();
        auto mobs = OktwCommon::GetMinions(p.ServerPosition(), m_Q.Range, false, true);
        if (mobs.empty()) return;
        const auto& mob = mobs.front();
        if (m_W.IsReady() && GetBool("jungleW")) { m_W.Cast(mob.ServerPosition()); return; }
        if (m_E.IsReady() && GetBool("jungleE")) { m_E.Cast(mob); return; }
        if (m_Q.IsReady() && GetBool("jungleQ")) { m_Q.Cast(mob); return; }
        if (m_R.IsReady() && GetBool("jungleR") && !m_Ractive) { m_R.Cast(); return; }
    }

    void OnGameDraw() override {}
};

} } // namespace Plugins::OKTW
