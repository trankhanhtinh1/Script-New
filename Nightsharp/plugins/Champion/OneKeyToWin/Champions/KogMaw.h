#pragma once
// Port of OKTW_CSharp/Champions/KogMaw.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;

class KogMawPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW KogMaw"; }
    const char* GetInternalId() const override { return "champion.oktw.kogmaw"; }
    const char* GetChampionName() const override { return "KogMaw"; }

protected:
    bool m_attackNow = true;

    void BuildMenu() override {
        MarkActive();

        m_Q = Spell(SpellSlot::Q, 980.0f);
        m_W = Spell(SpellSlot::W, 1000.0f);
        m_E = Spell(SpellSlot::E, 1200.0f);
        m_R = Spell(SpellSlot::R, 1800.0f);

        m_Q.SetSkillshot(0.25f, 50.0f, 2000.0f,  true,  SDK::SpellType::Line);
        m_E.SetSkillshot(0.25f, 120.0f, 1400.0f, false, SDK::SpellType::Line);
        m_R.SetSkillshot(1.2f,  120.0f, FLT_MAX, false, SDK::SpellType::Circle);

        m_qMenu->Add(new MenuBool("autoQ",   "Auto Q", true));
        m_qMenu->Add(new MenuBool("harassQ", "Harass Q", true));

        m_eMenu->Add(new MenuBool("autoE",   "Auto E", true));
        m_eMenu->Add(new MenuBool("harassE", "Harass E", true));
        m_eMenu->Add(new MenuBool("AGC",     "AntiGapcloserE", true));

        m_wMenu->Add(new MenuBool("autoW",   "Auto W", true));
        m_wMenu->Add(new MenuBool("harassW", "Harass W on max range", true));

        m_rMenu->Add(new MenuBool("autoR",       "Auto R", true));
        m_rMenu->Add(new MenuSlider("RmaxHp",    "Target max % HP", 50, 0, 100));
        m_rMenu->Add(new MenuSlider("comboStack","Max combo stack R", 2, 0, 10));
        m_rMenu->Add(new MenuSlider("harasStack","Max haras stack R", 1, 0, 10));
        m_rMenu->Add(new MenuBool("Rcc",         "R cc", true));
        m_rMenu->Add(new MenuBool("Rslow",       "R slow", true));
        m_rMenu->Add(new MenuBool("Raoe",        "R aoe", true));
        m_rMenu->Add(new MenuBool("Raa",         "R only out off AA range", false));

        m_drawMenu->Add(new MenuBool("ComboInfo", "R killable info", true));
        m_drawMenu->Add(new MenuBool("qRange",    "Q range", false));
        m_drawMenu->Add(new MenuBool("wRange",    "W range", false));
        m_drawMenu->Add(new MenuBool("eRange",    "E range", false));
        m_drawMenu->Add(new MenuBool("rRange",    "R range", false));
        m_drawMenu->Add(new MenuBool("onlyRdy",   "Draw only ready spells", true));

        m_champMenu->Add(new MenuBool("sheen",       "Sheen logic", true));
        m_champMenu->Add(new MenuBool("AApriority",  "AA priority over spell", true));

        m_farmMenu->Add(new MenuBool("farmW",   "LaneClear W", true));
        m_farmMenu->Add(new MenuBool("farmE",   "LaneClear E", true));
        m_farmMenu->Add(new MenuBool("jungleW", "Jungle clear W", true));
        m_farmMenu->Add(new MenuBool("jungleE", "Jungle clear E", true));

        // TODO(oktw-port): SebbyLib.Orbwalking.BeforeAttack / AfterAttack hooks
        // TODO(oktw-port): AntiGapcloser.OnEnemyGapcloser hook (AGC menu retained)
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
        const auto player = Player();
        if (LagFree(0)) {
            const int rLvl = static_cast<int>(player.GetSpell(SpellSlot::R).Level());
            const int wLvl = static_cast<int>(player.GetSpell(SpellSlot::W).Level());
            m_R.Range = 870.0f + 300.0f * static_cast<float>(rLvl);
            m_W.Range = 650.0f + 30.0f  * static_cast<float>(wLvl);
            SetMana();
            Jungle();
        }
        if (LagFree(1) && m_E.IsReady() && GetBool("autoE")) LogicE();
        if (LagFree(2) && m_Q.IsReady() && GetBool("autoQ")) LogicQ();
        if (LagFree(3) && m_W.IsReady() && GetBool("autoW")) LogicW();
        if (LagFree(4) && m_R.IsReady())                     LogicR();
    }

    void Jungle() {
        if (!LaneClear()) return;
        const auto p = Player();
        if (p.Mana() <= m_RMANA + m_QMANA) return;
        auto mobs = OktwCommon::GetMinions(p.ServerPosition(), 650.0f, false, true);
        if (mobs.empty()) return;
        const auto& mob = mobs.front();
        if (m_E.IsReady() && GetBool("jungleE")) { m_E.Cast(mob.ServerPosition()); return; }
        if (m_W.IsReady() && GetBool("jungleW")) { m_W.Cast(); return; }
    }

    void LogicR() {
        if (!GetBool("autoR") || !Sheen()) return;
        auto* ts = SDK::TargetSelector::Instance();
        auto target = ts ? ts->GetTarget(m_R.Range, DamageType::Magical) : AIHeroClient();
        if (!target.IsValid()) return;
        if (!SDK::Extensions::IsValidTarget(target, m_R.Range, true)) return;
        if (target.HealthPercent() >= static_cast<float>(GetSlider("RmaxHp", 50))) return;
        if (!OktwCommon::ValidUlt(target)) return;

        if (GetBool("Raa") && SDK::Core::Utils::AutoAttack::InAutoAttackRange(target)) return;

        const int harasStack = GetSlider("harasStack", 1);
        const int comboStack = GetSlider("comboStack", 2);
        const int countR     = GetRStacks();

        const auto p = Player();
        float rDmgBase = m_R.GetDamage(target);
        float rDmgAoe  = rDmgBase + static_cast<float>(OktwCommon::CountAlliesInRange(target.Position(), 500.0f)) * rDmgBase;

        if (m_R.GetDamage(target) > target.Health() - OktwCommon::GetIncomingDamage(target)) {
            CastSpell(m_R, target);
        } else if (Combo() && rDmgAoe * 2.0f > target.Health() && p.Mana() > m_RMANA * 3.0f) {
            CastSpell(m_R, target);
        } else if (countR < comboStack + 2 && p.Mana() > m_RMANA * 3.0f) {
            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                if (SDK::Extensions::IsValidTarget(enemy, m_R.Range, true) &&
                    !OktwCommon::CanMove(enemy)) {
                    m_R.Cast(enemy);
                }
            }
        }

        const bool targetSlowed = OktwCommon::HasBuffOfType(target, SDK::Prediction::BuffType::Snare) ||
                                  target.HasBuff("slow"); // approximation of HasBuffOfType(Slow)
        if (targetSlowed && GetBool("Rslow") && countR < comboStack + 1 &&
            p.Mana() > m_RMANA + m_WMANA + m_EMANA + m_QMANA) {
            CastSpell(m_R, target);
        } else if (Combo() && countR < comboStack &&
                   p.Mana() > m_RMANA + m_WMANA + m_EMANA + m_QMANA) {
            CastSpell(m_R, target);
        } else if (Harass() && countR < harasStack &&
                   p.Mana() > m_RMANA + m_WMANA + m_EMANA + m_QMANA) {
            CastSpell(m_R, target);
        }
    }

    void LogicW() {
        const auto p = Player();
        if (p.CountEnemyHeroesInRange(m_W.Range) > 0 && Sheen()) {
            if (Combo()) {
                m_W.Cast();
            } else if (Harass() && GetBool("harassW") &&
                       p.CountEnemyHeroesInRange(p.AttackRange()) > 0) {
                m_W.Cast();
            }
        }
    }

    void LogicQ() {
        if (!Sheen()) return;
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_Q.Range, DamageType::Physical) : AIHeroClient();
        if (!t.IsValid()) return;

        const auto p = Player();
        const float qDmg = OktwCommon::GetKsDamage(t, m_Q);
        const float eDmg = m_E.GetDamage(t);

        if (SDK::Extensions::IsValidTarget(t, m_W.Range, true) && qDmg + eDmg > t.Health()) {
            CastSpell(m_Q, t);
        } else if (Combo() && p.Mana() > m_RMANA + m_QMANA * 2.0f + m_EMANA) {
            CastSpell(m_Q, t);
        } else if (Harass() && p.Mana() > m_RMANA + m_EMANA + m_QMANA * 2.0f + m_WMANA &&
                   GetBool("harassQ") && !p.IsUnderEnemyTurret()) {
            CastSpell(m_Q, t);
        } else if ((Combo() || Harass()) && p.Mana() > m_RMANA + m_QMANA + m_EMANA) {
            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                if (SDK::Extensions::IsValidTarget(enemy, m_Q.Range, true) &&
                    !OktwCommon::CanMove(enemy)) {
                    m_Q.Cast(enemy);
                }
            }
        }
    }

    void LogicE() {
        if (!Sheen()) return;
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_E.Range, DamageType::Physical) : AIHeroClient();
        const auto p = Player();
        if (t.IsValid()) {
            const float qDmg = m_Q.GetDamage(t);
            const float eDmg = OktwCommon::GetKsDamage(t, m_E);
            if (eDmg > t.Health()) {
                CastSpell(m_E, t);
            } else if (eDmg + qDmg > t.Health() && m_Q.IsReady()) {
                CastSpell(m_E, t);
            } else if (Combo() && p.Mana() > m_RMANA + m_WMANA + m_EMANA + m_QMANA) {
                CastSpell(m_E, t);
            } else if (Harass() && GetBool("harassE") &&
                       p.Mana() > m_RMANA + m_WMANA + m_EMANA + m_QMANA + m_EMANA) {
                CastSpell(m_E, t);
            } else if ((Combo() || Harass()) && p.Mana() > m_RMANA + m_WMANA + m_EMANA) {
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
            auto farm = m_E.GetLineFarmLocation(baseList, m_E.Width);
            if (farm.MinionsHit >= FarmMinions()) m_E.Cast(farm.Position);
        }
    }

    // Sheen: honor Sheen proc / AA priority. Simplified — Orbwalker::GetTarget()
    // isn't available in SDK, so we approximate: sheen buff blocks spells if a
    // hero AA target is in range; AApriority blocks if we haven't just attacked.
    bool Sheen() {
        auto* ts = SDK::TargetSelector::Instance();
        auto aaTarget = ts ? ts->GetTarget(Player().AttackRange() + 65.0f, DamageType::Physical)
                           : AIHeroClient();
        const bool haveAaHero = aaTarget.IsValid();
        if (!haveAaHero) m_attackNow = true;
        if (haveAaHero && Player().HasBuff("sheen") && GetBool("sheen")) {
            return false;
        }
        if (haveAaHero && GetBool("AApriority") && !m_attackNow) {
            return false;
        }
        return true;
    }

    int GetRStacks() const {
        return Player().GetBuffCount("kogmawlivingartillerycost");
    }

    void OnGameDraw() override {
        // Simplified: rely on SDK draw utilities for range circles
    }
};

} } // namespace Plugins::OKTW
