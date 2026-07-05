#pragma once
// Port of OKTW_CSharp/Champions/Ziggs.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;
using SDK::UI::MenuKeyBind;

class ZiggsPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Ziggs"; }
    const char* GetInternalId() const override { return "champion.oktw.ziggs"; }
    const char* GetChampionName() const override { return "Ziggs"; }

protected:
    Spell m_Q1{ SpellSlot::Q };
    Spell m_Q2{ SpellSlot::Q };
    Spell m_Q3{ SpellSlot::Q };

    void BuildMenu() override {
        MarkActive();

        m_Q1 = Spell(SpellSlot::Q, 850.0f);
        m_Q2 = Spell(SpellSlot::Q, 1115.0f);
        m_Q3 = Spell(SpellSlot::Q, 1390.0f);
        m_Q  = m_Q1;

        m_W = Spell(SpellSlot::W, 1000.0f);
        m_E = Spell(SpellSlot::E, 900.0f);
        m_R = Spell(SpellSlot::R, 5300.0f);

        m_Q1.SetSkillshot(0.25f, 100.0f, 1700.0f, false, SDK::SpellType::Circle);
        m_Q2.SetSkillshot(0.4f,  50.0f,  1650.0f, false, SDK::SpellType::Circle);
        m_Q3.SetSkillshot(0.6f,  50.0f,  1650.0f, false, SDK::SpellType::Circle);

        m_W.SetSkillshot(0.25f, 275.0f, 1750.0f, false, SDK::SpellType::Circle);
        m_E.SetSkillshot(1.0f,  150.0f, 1750.0f, false, SDK::SpellType::Circle);
        m_R.SetSkillshot(0.7f,  350.0f, 1500.0f, false, SDK::SpellType::Circle);

        m_qMenu->Add(new MenuBool("autoQ",        "Auto Q", true));
        m_qMenu->Add(new MenuBool("harassQ",      "Harass Q", true));
        m_qMenu->Add(new MenuSlider("QHarassMana", "Harass Mana", 30, 0, 100));

        m_wMenu->Add(new MenuBool("autoW",       "Auto W", true));
        m_wMenu->Add(new MenuBool("harassW",     "Harass W", true));
        m_wMenu->Add(new MenuBool("interupterW", "Interrupter W", true));

        m_eMenu->Add(new MenuBool("autoE",  "Auto E on CC", true));
        m_eMenu->Add(new MenuBool("comboE", "Auto E in Combo BETA", true));
        m_eMenu->Add(new MenuBool("AGC",    "Anti Gapcloser E", true));
        m_eMenu->Add(new MenuBool("opsE",   "OnProcessSpellCastE", true));
        m_eMenu->Add(new MenuBool("telE",   "Auto E teleport", true));

        m_farmMenu->Add(new MenuBool("farmQout", "Last hit Q minion out range AA", true));
        m_farmMenu->Add(new MenuBool("farmQ",    "Lane clear Q", true));
        m_farmMenu->Add(new MenuBool("farmE",    "Lane clear E", true));
        m_farmMenu->Add(new MenuBool("farmW",    "Lane clear W", true));
        m_farmMenu->Add(new MenuBool("jungleE",  "Jungle clear E", true));
        m_farmMenu->Add(new MenuBool("jungleQ",  "Jungle clear Q", true));
        m_farmMenu->Add(new MenuBool("jungleW",  "Jungle clear W", true));

        m_rMenu->Add(new MenuBool("autoR",     "Auto R", true));
        m_rMenu->Add(new MenuBool("Rcc",       "R cc", true));
        m_rMenu->Add(new MenuSlider("Raoe",    "R AOE", 3, 0, 5));

        Menu* jungle = m_rMenu->AddSubMenu(new Menu("RJungleSub", "R Jungle stealer"));
        jungle->Add(new MenuBool("Rjungle", "R Jungle stealer", true));
        jungle->Add(new MenuBool("Rdragon", "Dragon", true));
        jungle->Add(new MenuBool("Rbaron",  "Baron", true));
        jungle->Add(new MenuBool("Rred",    "Red", true));
        jungle->Add(new MenuBool("Rblue",   "Blue", true));
        jungle->Add(new MenuBool("Rally",   "Ally stealer", false));

        m_rMenu->Add(new MenuKeyBind("useR", "Semi-manual cast R key", 'T', SDK::KeyBindType::Press));
        m_rMenu->Add(new MenuBool("Rturrent",    "Don't R under turret", true));
        m_rMenu->Add(new MenuSlider("MaxRangeR", "Max R range", 3000, 0, 5000));
        m_rMenu->Add(new MenuSlider("MinRangeR", "Min R range", 900,  0, 5000));

        // TODO(oktw-port): GameObject.OnCreate/OnDelete, Interrupter2, AntiGapcloser events not available
    }

    void SetMana() override {
        if ((Shared().manaDisable && Shared().manaDisable->Value && Combo()) ||
            Player().HealthPercent() < 20.0f) {
            m_QMANA = m_WMANA = m_EMANA = m_RMANA = 0.0f;
            return;
        }
        m_QMANA = m_Q1.Instance().ManaCost();
        m_WMANA = m_W.Instance().ManaCost();
        m_EMANA = m_E.Instance().ManaCost();
        m_RMANA = m_R.IsReady() ? m_R.Instance().ManaCost() : m_QMANA;
    }

    void OnGameUpdate() override {
        if (LagFree(0)) SetMana();
        if (LagFree(1) && m_E.IsReady()) LogicE();
        if (LagFree(2) && m_Q1.IsReady() && GetBool("autoQ")) LogicQ();
        if (LagFree(3) && m_W.IsReady() && GetBool("autoW")) LogicW();

        if (m_R.IsReady()) {
            if (LagFree(4) && GetBool("autoR")) LogicR();

            if (GetKey("useR")) {
                auto* ts = SDK::TargetSelector::Instance();
                auto t = ts ? ts->GetTarget(2500.0f, DamageType::Magical) : AIHeroClient();
                if (t.IsValid()) m_R.Cast(t);
            }
        }
    }

    void LogicQ() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_Q3.Range, DamageType::Magical) : AIHeroClient();
        const auto p = Player();
        if (t.IsValid()) {
            if (Combo() && p.Mana() > m_RMANA + m_QMANA + m_EMANA) {
                CastQ(t);
            } else if (Harass() && GetBool("harassQ") &&
                       GetBool((std::string("Harass") + t.CharacterName()).c_str()) &&
                       p.ManaPercent() > GetSlider("QHarassMana", 30) &&
                       OktwCommon::CanHarras()) {
                CastQ(t);
            } else if (OktwCommon::GetKsDamage(t, m_Q1) > t.Health()) {
                CastQ(t);
            } else if (p.Mana() > m_RMANA + m_QMANA) {
                for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                    if (enemy.IsValid() && enemy.IsEnemy() &&
                        SDK::Extensions::IsValidTarget(enemy, m_Q3.Range, true) &&
                        !OktwCommon::CanMove(enemy)) {
                        CastQ(t);
                    }
                }
            }
        }

        if (!None() && !Combo()) {
            auto minions = OktwCommon::GetMinions(p.ServerPosition(), m_Q1.Range);

            if (GetBool("farmQout") && p.Mana() > m_RMANA + m_QMANA + m_EMANA + m_WMANA) {
                for (const auto& mn : minions) {
                    if (!SDK::Extensions::IsValidTarget(mn, m_Q1.Range, true)) continue;
                    if (mn.Health() < m_Q1.GetDamage(mn)) {
                        m_Q1.Cast(mn);
                        return;
                    }
                }
            }
            if (FarmSpells() && GetBool("farmQ")) {
                std::vector<SDK::AIBaseClient> baseList(minions.begin(), minions.end());
                auto farm = m_Q1.GetCircularFarmLocation(baseList, m_Q1.Width);
                if (farm.MinionsHit >= FarmMinions()) m_Q1.Cast(farm.Position);
            }
        }
    }

    void LogicW() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_W.Range - 250.0f, DamageType::Magical) : AIHeroClient();
        const auto p = Player();
        if (t.IsValid()) {
            // Close melee push
            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                if (enemy.IsMelee() && SDK::Extensions::IsValidTarget(enemy, 350.0f, true)) {
                    const Vector3 dir = (enemy.Position() - p.Position());
                    const float len = dir.Length();
                    if (len > 0.001f) {
                        Vector3 pushPos = p.Position() + dir * (50.0f / len);
                        m_W.Cast(pushPos);
                    }
                    break;
                }
            }

            const auto pred = m_W.GetPrediction(t, true);
            const Vector3 castPos = pred.GetCastPosition();
            const float tP = p.Position().Distance(t.Position());
            const float tC = p.Position().Distance(castPos);
            if (Combo() && p.Mana() > m_RMANA + m_WMANA) {
                const Vector3 dir = (castPos - p.Position());
                const float len = dir.Length();
                if (len > 0.001f) {
                    if (tP < tC && tP > 500.0f) {
                        m_W.Cast(p.Position() + dir * ((tP + 250.0f) / len));
                    } else if (tP > tC && tP < 500.0f) {
                        m_W.Cast(p.Position() + dir * ((tP - 250.0f) / len));
                    }
                }
            }
        }
    }

    void LogicE() {
        const auto p = Player();
        if (p.Mana() > m_RMANA + m_EMANA && GetBool("autoE")) {
            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                if (enemy.IsMelee() && SDK::Extensions::IsValidTarget(enemy, 400.0f, true)) {
                    m_E.Cast(enemy);
                    break;
                }
            }

            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (enemy.IsValid() && enemy.IsEnemy() &&
                    SDK::Extensions::IsValidTarget(enemy, m_E.Range + 50.0f, true) &&
                    !OktwCommon::CanMove(enemy)) {
                    m_E.Cast(enemy);
                    return;
                }
            }

            if (Combo() && GetBool("comboE") && p.Mana() > m_RMANA + m_EMANA + m_WMANA) {
                auto* ts = SDK::TargetSelector::Instance();
                auto t = ts ? ts->GetTarget(m_E.Range, DamageType::Physical) : AIHeroClient();
                if (SDK::Extensions::IsValidTarget(t, m_E.Range, true)) {
                    auto pr = m_E.GetPrediction(t, true);
                    if (pr.AoeTargetsHitCount >= 2) m_E.Cast(pr.GetCastPosition());
                    if (OktwCommon::IsMovingInSameDirection(p, t)) {
                        CastSpell(m_E, t);
                    }
                }
            }
        }
    }

    void LogicR() {
        const auto p = Player();
        if (GetBool("autoR") && OktwCommon::CountEnemiesInRange(p.Position(), 800.0f) == 0) {
            m_R.Range = static_cast<float>(GetSlider("MaxRangeR", 3000));
            for (const auto& target : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!target.IsValid() || !target.IsEnemy()) continue;
                if (!SDK::Extensions::IsValidTarget(target, m_R.Range, true)) continue;
                if (!OktwCommon::ValidUlt(target)) continue;

                const float predictedHealth = target.Health() - OktwCommon::GetIncomingDamage(target);
                float rDmg = m_R.GetDamage(target);

                if (rDmg > predictedHealth) {
                    const float incom = OktwCommon::GetIncomingDamage(p);
                    if (incom > 0.0f && p.Health() - incom < p.Level() * 12.0f)
                        m_R.Cast(target);
                }

                if (GetBool("Rcc") && rDmg > predictedHealth && !OktwCommon::CanMove(target)) {
                    m_R.Cast(target);
                }
                rDmg = rDmg * 0.66f;

                if (rDmg > predictedHealth &&
                    OktwCommon::CountAlliesInRange(target.Position(), 500.0f) == 0 &&
                    p.Position().Distance(target.Position()) > GetSlider("MinRangeR", 900)) {
                    CastSpell(m_R, target);
                }
                if (Combo()) {
                    auto pr = m_R.GetPrediction(target, true);
                    if (pr.AoeTargetsHitCount >= GetSlider("Raoe", 3))
                        m_R.Cast(pr.GetCastPosition());
                }
            }
        }
    }

    void CastQ(const SDK::AIBaseClient& target) {
        // Multi-range Q variant: try nearest range first
        const float dist = Player().Position().Distance(target.Position());
        Spell* pick = &m_Q1;
        if (dist > m_Q2.Range) pick = &m_Q3;
        else if (dist > m_Q1.Range) pick = &m_Q2;

        auto pr = pick->GetPrediction(target, true);
        if (pr.Hitchance >= SDK::HitChance::High) {
            pick->Cast(pr.GetCastPosition());
        }
    }

    void OnGameDraw() override {}
};

} } // namespace Plugins::OKTW
