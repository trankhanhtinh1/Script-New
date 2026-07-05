#pragma once
// Port of OKTW_CSharp/Champions/Ahri.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;

class AhriPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Ahri"; }
    const char* GetInternalId() const override { return "champion.oktw.ahri"; }
    const char* GetChampionName() const override { return "Ahri"; }

protected:
    void BuildMenu() override {
        MarkActive();

        m_Q = Spell(SpellSlot::Q, 870.0f);
        m_W = Spell(SpellSlot::W, 580.0f);
        m_E = Spell(SpellSlot::E, 950.0f);
        m_R = Spell(SpellSlot::R, 600.0f);
        m_Q.SetSkillshot(0.25f, 90.0f, 1550.0f, false, SDK::SpellType::Line);
        m_E.SetSkillshot(0.25f, 60.0f, 1550.0f, true,  SDK::SpellType::Line);

        m_drawMenu->Add(new MenuBool("noti",    "Show notification & line", true));
        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw only ready spells",   true));
        m_drawMenu->Add(new MenuBool("qRange",  "Q range", false));
        m_drawMenu->Add(new MenuBool("wRange",  "W range", false));
        m_drawMenu->Add(new MenuBool("eRange",  "E range", false));
        m_drawMenu->Add(new MenuBool("rRange",  "R range", false));

        m_qMenu->Add(new MenuBool("autoQ",   "Auto Q", true));
        m_qMenu->Add(new MenuBool("harassQ", "Harass Q", true));

        m_wMenu->Add(new MenuBool("autoW",   "Auto W", true));
        m_wMenu->Add(new MenuBool("harassW", "Harass W", true));

        m_eMenu->Add(new MenuBool("autoE",   "Auto E", true));
        m_eMenu->Add(new MenuBool("harassE", "Harass E", true));

        Menu* eon  = m_eMenu->AddSubMenu(new Menu("EonSub",  "Use E on"));
        Menu* egap = m_eMenu->AddSubMenu(new Menu("EgapSub", "Gapcloser"));
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string idE = std::string("Eon")        + enemy.CharacterName();
            const std::string idG = std::string("Egapcloser") + enemy.CharacterName();
            eon->Add(new MenuBool(idE.c_str(),  enemy.CharacterName().c_str(), true));
            egap->Add(new MenuBool(idG.c_str(), enemy.CharacterName().c_str(), true));
        }

        m_rMenu->Add(new MenuBool("autoR",  "R KS", true));
        m_rMenu->Add(new MenuBool("autoR2", "auto R fight logic + aim Q", true));

        m_farmMenu->Add(new MenuBool("farmQ",    "Lane clear Q", true));
        m_farmMenu->Add(new MenuBool("farmW",    "Lane clear W", false));
        m_farmMenu->Add(new MenuBool("jungleQ",  "Jungle clear Q", true));
        m_farmMenu->Add(new MenuBool("jungleW",  "Jungle clear W", true));
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
        if (LagFree(0)) { SetMana(); Jungle(); }
        if (m_E.IsReady() && GetBool("autoE")) LogicE();
        if (LagFree(2) && m_W.IsReady() && GetBool("autoW")) LogicW();
        if (LagFree(3) && m_Q.IsReady() && GetBool("autoQ")) LogicQ();
        if (LagFree(4) && m_R.IsReady() && Combo()) LogicR();
    }

    void LogicR() {
        const auto player = Player();
        Vector3 dashPosition = player.Position();
        const Vector3 cursor = SDK::Game::CursorPos();
        if (player.Position().Distance(cursor) < 450.0f) dashPosition = cursor;
        else {
            const Vector3 dir = (cursor - player.Position());
            const float len = dir.Length();
            if (len > 0.001f) dashPosition = player.Position() + dir * (450.0f / len);
        }
        if (OktwCommon::CountEnemiesInRange(dashPosition, 800.0f) > 2) return;

        if (GetBool("autoR2") && player.HasBuff("AhriTumble")) {
            const float bt = OktwCommon::GetPassiveTime(player, "AhriTumble");
            if (bt < 3.0f) m_R.Cast(dashPosition);
        }

        if (GetBool("autoR")) {
            auto* ts = SDK::TargetSelector::Instance();
            auto t = ts ? ts->GetTarget(450.0f + m_R.Range, DamageType::Magical) : AIHeroClient();
            if (t.IsValid()) {
                float comboDmg = m_R.GetDamage(t) * 3.0f;
                if (m_Q.IsReady()) comboDmg += m_Q.GetDamage(t) * 2.0f;
                if (m_W.IsReady()) comboDmg += m_W.GetDamage(t) + m_W.GetDamage(t, SDK::DamageStage::SecondForm);
                if (OktwCommon::CountAlliesInRange(t.Position(), 600.0f) < 2 &&
                    comboDmg > t.Health() &&
                    t.Position().Distance(cursor) < t.Position().Distance(player.Position()) &&
                    dashPosition.Distance(t.ServerPosition()) < 500.0f) {
                    m_R.Cast(dashPosition);
                }
                for (const auto& target : SDK::ObjectManager::Get<AIHeroClient>()) {
                    if (target.IsValid() && target.IsEnemy() && target.IsMelee() &&
                        SDK::Extensions::IsValidTarget(target, 300.0f, true)) {
                        m_R.Cast(dashPosition);
                        break;
                    }
                }
            }
        }
    }

    void LogicW() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_W.Range, DamageType::Magical) : AIHeroClient();
        if (t.IsValid()) {
            const auto p = Player();
            const float mana = p.Mana();
            if (Combo() && mana > m_RMANA + m_WMANA) {
                m_W.Cast();
            } else if (Harass() && mana > m_RMANA + m_QMANA + m_WMANA &&
                       GetBool("harassW") &&
                       GetBool((std::string("Harass") + t.CharacterName()).c_str())) {
                m_W.Cast();
            } else if (m_W.GetDamage(t) + m_W.GetDamage(t, SDK::DamageStage::SecondForm) +
                       m_Q.GetDamage(t) * 2.0f > t.Health() - OktwCommon::GetIncomingDamage(t)) {
                m_W.Cast();
            }
        } else if (FarmSpells() && GetBool("farmW")) {
            const auto minions = OktwCommon::GetMinions(Player().ServerPosition(), m_W.Range, true);
            int killable = 0;
            for (const auto& mn : minions) if (mn.Health() < m_W.GetDamage(mn)) ++killable;
            if (static_cast<int>(minions.size()) >= FarmMinions() && killable > 0)
                m_W.Cast();
        }
    }

    void LogicQ() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_Q.Range, DamageType::Magical) : AIHeroClient();
        const auto p = Player();
        if (t.IsValid()) {
            if (Combo() && p.Mana() > m_RMANA + m_QMANA) {
                CastSpell(m_Q, t);
            } else if (Harass() && p.Mana() > m_RMANA + m_WMANA + m_QMANA + m_QMANA &&
                       GetBool("harassQ") &&
                       GetBool((std::string("Harass") + t.CharacterName()).c_str()) &&
                       OktwCommon::CanHarras()) {
                CastSpell(m_Q, t);
            } else if (m_Q.GetDamage(t) * 2.0f + OktwCommon::GetEchoLudenDamage(t) >
                       t.Health() - OktwCommon::GetIncomingDamage(t)) {
                m_Q.Cast(t);
            }

            if (!None() && p.Mana() > m_RMANA + m_WMANA) {
                for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                    if (enemy.IsValid() && enemy.IsEnemy() &&
                        SDK::Extensions::IsValidTarget(enemy, m_Q.Range, true) &&
                        !OktwCommon::CanMove(enemy)) {
                        m_Q.Cast(enemy);
                    }
                }
            }
        } else if (FarmSpells() && GetBool("farmQ")) {
            auto minions = OktwCommon::GetMinions(Player().ServerPosition(), m_Q.Range);
            std::vector<SDK::AIBaseClient> baseList(minions.begin(), minions.end());
            auto farm = m_Q.GetLineFarmLocation(baseList, m_Q.Width);
            if (farm.MinionsHit >= FarmMinions()) m_Q.Cast(farm.Position);
        }
    }

    void LogicE() {
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            if (SDK::Extensions::IsValidTarget(enemy, m_E.Range, true) &&
                OktwCommon::GetKsDamage(enemy, m_E) + m_Q.GetDamage(enemy) + m_W.GetDamage(enemy) > enemy.Health()) {
                CastSpell(m_E, enemy);
            }
        }

        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_E.Range, DamageType::Magical) : AIHeroClient();
        const auto p = Player();
        if (t.IsValid()) {
            const std::string eonId = std::string("Eon") + t.CharacterName();
            if (Combo() && p.Mana() > m_RMANA + m_EMANA && GetBool(eonId.c_str())) {
                CastSpell(m_E, t);
            } else if (Harass() && p.Mana() > m_RMANA + m_EMANA + m_WMANA + m_EMANA &&
                       GetBool("harassE") &&
                       GetBool((std::string("Harass") + t.CharacterName()).c_str())) {
                CastSpell(m_E, t);
            }
            if (!None() && p.Mana() > m_RMANA + m_EMANA) {
                for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                    if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                    const std::string id2 = std::string("Eon") + enemy.CharacterName();
                    if (SDK::Extensions::IsValidTarget(enemy, m_E.Range, true) &&
                        !OktwCommon::CanMove(enemy) && GetBool(id2.c_str())) {
                        m_E.Cast(enemy);
                    }
                }
            }
        }
    }

    void Jungle() {
        if (!LaneClear()) return;
        const auto p = Player();
        if (p.Mana() <= m_QMANA + m_RMANA) return;
        auto mobs = OktwCommon::GetMinions(p.ServerPosition(), 600.0f, false, true);
        if (mobs.empty()) return;
        const auto& mob = mobs.front();
        if (m_W.IsReady() && GetBool("jungleW")) { m_W.Cast(); return; }
        if (m_Q.IsReady() && GetBool("jungleQ")) { m_Q.Cast(mob.Position()); return; }
    }

    void OnGameDraw() override {
        // Simplified: rely on SDK draw utilities for range circles
    }
};

} } // namespace Plugins::OKTW
