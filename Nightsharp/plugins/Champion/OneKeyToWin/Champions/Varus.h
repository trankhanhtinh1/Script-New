#pragma once
// Port of OKTW_CSharp/Champions/Varus.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;
using SDK::UI::MenuKeyBind;

class VarusPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Varus"; }
    const char* GetInternalId() const override { return "champion.oktw.varus"; }
    const char* GetChampionName() const override { return "Varus"; }

protected:
    float m_castTime = 0.0f;
    bool  m_canCast  = true;

    void BuildMenu() override {
        MarkActive();

        m_Q = Spell(SpellSlot::Q, 925.0f);
        m_W = Spell(SpellSlot::Q, 0.0f);
        m_E = Spell(SpellSlot::E, 975.0f);
        m_R = Spell(SpellSlot::R, 1050.0f);

        m_Q.SetSkillshot(0.25f, 70.0f,  1650.0f, false, SDK::SpellType::Line);
        m_E.SetSkillshot(0.35f, 120.0f, 1500.0f, false, SDK::SpellType::Circle);
        m_R.SetSkillshot(0.25f, 120.0f, 1950.0f, false, SDK::SpellType::Line);
        // TODO(oktw-port): Q charged spell - m_Q.SetCharged(925, 1600, 1.5f)

        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw only ready spells", true));
        m_drawMenu->Add(new MenuBool("qRange",  "Q range", false));
        m_drawMenu->Add(new MenuBool("eRange",  "E range", false));
        m_drawMenu->Add(new MenuBool("rRange",  "R range", false));

        m_qMenu->Add(new MenuBool("autoQ", "Auto Q", true));
        m_qMenu->Add(new MenuBool("maxQ",  "Cast Q only max range", true));
        m_qMenu->Add(new MenuBool("fastQ", "Fast cast Q", false));

        m_eMenu->Add(new MenuBool("autoE", "Auto E", true));

        m_rMenu->Add(new MenuBool("autoR", "Auto R", true));
        m_rMenu->Add(new MenuSlider("rCount", "Auto R if enemies in range (combo mode)", 3, 0, 5));
        m_rMenu->Add(new MenuKeyBind("useR", "Semi-manual cast R key", 'T', SDK::KeyBindType::Press));

        Menu* gap = m_rMenu->AddSubMenu(new Menu("GapCloserRSub", "GapCloser R"));
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string id = std::string("GapCloser") + enemy.CharacterName();
            gap->Add(new MenuBool(id.c_str(), enemy.CharacterName().c_str(), false));
        }

        m_farmMenu->Add(new MenuBool("farmQ", "Lane clear Q", true));
        m_farmMenu->Add(new MenuBool("farmE", "Lane clear E", true));

        // TODO(oktw-port): Obj_AI_Base.OnProcessSpellCast, AntiGapcloser events not available
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

    float GetWDmg(const SDK::AIBaseClient& target) {
        // varuswdebuff stack count not available - approximate
        return m_W.GetDamage(target);
    }

    void OnGameUpdate() override {
        if (m_R.IsReady() && GetKey("useR")) {
            auto* ts = SDK::TargetSelector::Instance();
            auto t = ts ? ts->GetTarget(m_R.Range, DamageType::Physical) : AIHeroClient();
            if (t.IsValid()) m_R.Cast(t);
        }

        if (LagFree(0)) {
            SetMana();
            if (!m_canCast) {
                // Simplified: no Game.Time available same way, allow cast quickly
                m_canCast = true;
            }
        }

        if (LagFree(1) && m_E.IsReady() && GetBool("autoQ")) LogicE();
        if (LagFree(2) && m_Q.IsReady() && GetBool("autoE")) LogicQ();
        if (LagFree(3) && m_R.IsReady() && GetBool("autoR")) LogicR();
        if (LagFree(4)) Farm();
    }

    void Farm() {
        if (LaneClear() && m_E.IsReady() && GetBool("farmE")) {
            auto mobs = OktwCommon::GetMinions(Player().ServerPosition(), m_E.Range, false, true);
            if (!mobs.empty() && Player().Mana() > m_RMANA + m_EMANA + m_QMANA) {
                m_E.Cast(mobs.front());
                return;
            }
            if (FarmSpells()) {
                auto minions = OktwCommon::GetMinions(Player().ServerPosition(), m_E.Range);
                std::vector<SDK::AIBaseClient> baseList(minions.begin(), minions.end());
                auto farm = m_Q.GetCircularFarmLocation(baseList, m_E.Width);
                if (farm.MinionsHit > 3) {
                    m_E.Cast(farm.Position);
                    return;
                }
            }
        }
    }

    void LogicR() {
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(enemy, m_R.Range, true)) continue;

            const int rCount = GetSlider("rCount", 3);
            if (rCount > 0 && OktwCommon::CountEnemiesInRange(enemy.Position(), 400.0f) >= rCount) {
                m_R.Cast(enemy);
            }
            if ((OktwCommon::CountAlliesInRange(enemy.Position(), 600.0f) == 0 ||
                 Player().Health() < Player().MaxHealth() * 0.5f) &&
                m_R.GetDamage(enemy) + GetWDmg(enemy) + m_Q.GetDamage(enemy) > enemy.Health() &&
                OktwCommon::ValidUlt(enemy)) {
                CastSpell(m_R, enemy);
            }
        }
        if (Player().Health() < Player().MaxHealth() * 0.5f) {
            for (const auto& target : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!target.IsValid() || !target.IsEnemy()) continue;
                const std::string gcId = std::string("GapCloser") + target.CharacterName();
                if (SDK::Extensions::IsValidTarget(target, 270.0f, true) && target.IsMelee() &&
                    GetBool(gcId.c_str())) {
                    CastSpell(m_R, target);
                }
            }
        }
    }

    void LogicQ() {
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(enemy, 1600.0f, true)) continue;
            if (m_Q.GetDamage(enemy) + GetWDmg(enemy) > enemy.Health()) {
                if (SDK::Extensions::IsValidTarget(enemy, m_R.Range, true)) CastQ(enemy);
                return;
            }
        }

        if (GetBool("maxQ") && m_Q.Range < 1500.0f &&
            OktwCommon::CountEnemiesInRange(Player().Position(), Player().AttackRange() + Player().BoundingRadius() * 2.0f) == 0)
            return;

        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_Q.Range, DamageType::Physical) : AIHeroClient();
        const auto p = Player();
        if (t.IsValid()) {
            if (Combo() && p.Mana() > m_RMANA + m_QMANA) {
                CastQ(t);
            } else if (Harass() && p.Mana() > m_RMANA + m_EMANA + m_QMANA + m_QMANA &&
                       GetBool((std::string("Harass") + t.CharacterName()).c_str()) &&
                       OktwCommon::CanHarras()) {
                CastQ(t);
            } else if (!None() && p.Mana() > m_RMANA + m_WMANA) {
                for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                    if (enemy.IsValid() && enemy.IsEnemy() &&
                        SDK::Extensions::IsValidTarget(enemy, m_Q.Range, true) &&
                        !OktwCommon::CanMove(enemy)) {
                        CastQ(enemy);
                    }
                }
            }
        } else if (FarmSpells() && GetBool("farmQ") && m_Q.Range > 1500.0f &&
                   OktwCommon::CountEnemiesInRange(Player().Position(), 1450.0f) == 0) {
            auto minions = OktwCommon::GetMinions(Player().ServerPosition(), m_Q.Range);
            std::vector<SDK::AIBaseClient> baseList(minions.begin(), minions.end());
            auto farm = m_Q.GetLineFarmLocation(baseList, m_Q.Width);
            if (farm.MinionsHit > 3) m_Q.Cast(farm.Position);
        }
    }

    void LogicE() {
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            if (SDK::Extensions::IsValidTarget(enemy, m_E.Range, true) &&
                m_E.GetDamage(enemy) + GetWDmg(enemy) > enemy.Health()) {
                CastSpell(m_E, enemy);
            }
        }
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_E.Range, DamageType::Physical) : AIHeroClient();
        const auto p = Player();
        if (t.IsValid()) {
            if (Combo() && p.Mana() > m_RMANA + m_QMANA) {
                CastSpell(m_E, t);
            } else if (!None() && p.Mana() > m_RMANA + m_WMANA) {
                for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                    if (enemy.IsValid() && enemy.IsEnemy() &&
                        SDK::Extensions::IsValidTarget(enemy, m_E.Range, true) &&
                        !OktwCommon::CanMove(enemy)) {
                        m_E.Cast(enemy);
                    }
                }
            }
        }
    }

    void CastQ(const SDK::AIBaseClient& target) {
        // TODO(oktw-port): Q is a charged spell - fallback to direct cast
        m_Q.Cast(target);
    }

    void OnGameDraw() override {}
};

} } // namespace Plugins::OKTW
