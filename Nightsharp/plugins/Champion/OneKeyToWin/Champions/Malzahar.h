#pragma once
// Port of OKTW_CSharp/Champions/Malzahar.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;
using SDK::UI::MenuKeyBind;

class MalzaharPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Malzahar"; }
    const char* GetInternalId() const override { return "champion.oktw.malzahar"; }
    const char* GetChampionName() const override { return "Malzahar"; }

protected:
    Spell m_Q1{ SpellSlot::Q };
    float m_rTime = 0.0f;

    void BuildMenu() override {
        MarkActive();

        m_Q  = Spell(SpellSlot::Q, 900.0f);
        m_Q1 = Spell(SpellSlot::Q, 900.0f);
        m_W  = Spell(SpellSlot::W, 750.0f);
        m_E  = Spell(SpellSlot::E, 650.0f);
        m_R  = Spell(SpellSlot::R, 700.0f);

        m_Q1.SetSkillshot(0.25f, 100.0f, FLT_MAX, false, SDK::SpellType::Circle);
        m_Q.SetSkillshot (0.75f, 80.0f,  FLT_MAX, false, SDK::SpellType::Circle);
        m_W.SetSkillshot (1.20f, 230.0f, FLT_MAX, false, SDK::SpellType::Circle);

        m_drawMenu->Add(new MenuBool("noti",    "Show notification & line", true));
        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw only ready spells",   true));
        m_drawMenu->Add(new MenuBool("qRange",  "Q range", false));
        m_drawMenu->Add(new MenuBool("wRange",  "W range", false));
        m_drawMenu->Add(new MenuBool("eRange",  "E range", false));
        m_drawMenu->Add(new MenuBool("rRange",  "R range", false));

        m_qMenu->Add(new MenuBool("autoQ",   "Auto Q",              true));
        m_qMenu->Add(new MenuBool("harassQ", "Harass Q",            true));
        m_qMenu->Add(new MenuBool("intQ",    "Interrupt spells Q",  true));
        m_qMenu->Add(new MenuBool("gapQ",    "Gapcloser Q",         true));

        m_wMenu->Add(new MenuBool("autoW",   "Auto W",   true));
        m_wMenu->Add(new MenuBool("harassW", "Harass W", true));

        m_eMenu->Add(new MenuBool("autoE",        "Auto E",                    true));
        m_eMenu->Add(new MenuBool("harassE",      "Harass E",                  true));
        m_eMenu->Add(new MenuBool("harrasEminion","Try harras E on minion",    true));

        m_rMenu->Add(new MenuBool   ("autoR",    "Auto R",                    true));
        m_rMenu->Add(new MenuBool   ("Rturrent", "Don't R under turret",      true));
        m_rMenu->Add(new MenuKeyBind("smartR",   "Semi-manual cast R key",    'T', SDK::KeyBindType::Press));
        m_rMenu->Add(new MenuKeyBind("useR",     "Fast combo key",            'T', SDK::KeyBindType::Press));

        Menu* rgap = m_rMenu->AddSubMenu(new Menu("RGapSub", "Gapcloser"));
        Menu* rFast = m_rMenu->AddSubMenu(new Menu("RFastSub", "Fast combo key use on:"));
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string idG = std::string("gapcloser") + enemy.CharacterName();
            const std::string idR = std::string("Ron")       + enemy.CharacterName();
            rgap->Add (new MenuBool(idG.c_str(), enemy.CharacterName().c_str(), false));
            rFast->Add(new MenuBool(idR.c_str(), enemy.CharacterName().c_str(), true));
        }

        m_farmMenu->Add(new MenuBool("farmQ",   "Lane clear Q", true));
        m_farmMenu->Add(new MenuBool("farmW",   "Lane clear W", true));
        m_farmMenu->Add(new MenuBool("farmE",   "Lane clear E", true));
        m_farmMenu->Add(new MenuBool("jungleE", "Jungle clear E", true));
        m_farmMenu->Add(new MenuBool("jungleQ", "Jungle clear Q", true));
        m_farmMenu->Add(new MenuBool("jungleW", "Jungle clear W", true));
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
        const auto p = Player();
        if (p.HasBuff("malzaharrsound")) return;

        if (m_R.IsReady() && GetKey("useR")) {
            auto* ts = SDK::TargetSelector::Instance();
            auto t = ts ? ts->GetTarget(m_R.Range, DamageType::Magical) : AIHeroClient();
            if (t.IsValid() && GetBool((std::string("Ron") + t.CharacterName()).c_str()) &&
                OktwCommon::ValidUlt(t)) {
                m_R.Cast(t);
                return;
            }
        }
        if (m_R.IsReady() && GetKey("smartR")) {
            auto* ts = SDK::TargetSelector::Instance();
            auto t = ts ? ts->GetTarget(m_R.Range, DamageType::Magical) : AIHeroClient();
            if (t.IsValid() && OktwCommon::ValidUlt(t)) {
                m_R.Cast(t);
                return;
            }
        }

        if (LagFree(0)) { SetMana(); Jungle(); }

        if (LagFree(1) && m_E.IsReady() && GetBool("autoE")) LogicE();
        if (LagFree(2) && m_Q.IsReady() && GetBool("autoQ")) LogicQ();
        if (LagFree(3) && m_W.IsReady() && GetBool("autoW")) LogicW();
        if (LagFree(4) && m_R.IsReady() && GetBool("autoR")) LogicR();

        // TODO(oktw-port): AntiGapcloser (gapQ / gapcloserX) -- OnEnemyGapcloser event unavailable
        // TODO(oktw-port): Interrupter2 (intQ) -- OnInterruptableTarget event unavailable
        // TODO(oktw-port): Spellbook.OnCastSpell R replacement combo unavailable
    }

    void LogicQ() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_Q.Range, DamageType::Magical) : AIHeroClient();
        const auto p = Player();

        if (t.IsValid()) {
            const float qDmg = OktwCommon::GetKsDamage(t, m_Q) + BonusDmg(t);
            if (qDmg > t.Health()) CastSpell(m_Q, t);

            if (m_R.IsReady() && SDK::Extensions::IsValidTarget(t, m_R.Range, true)) return;

            if (Combo() && p.Mana() > m_RMANA + m_QMANA) {
                CastSpell(m_Q, t);
            } else if (Harass() && GetBool("harassQ") &&
                       p.Mana() > m_RMANA + m_EMANA + m_WMANA + m_EMANA &&
                       GetBool((std::string("Harass") + t.CharacterName()).c_str())) {
                CastSpell(m_Q, t);
            }

            if (p.Mana() > m_RMANA + m_QMANA) {
                for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                    if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                    if (SDK::Extensions::IsValidTarget(enemy, m_Q.Range, true) &&
                        !OktwCommon::CanMove(enemy)) {
                        m_Q.Cast(enemy);
                    }
                }
            }
        } else if (FarmSpells() && GetBool("farmQ")) {
            auto minions = OktwCommon::GetMinions(p.ServerPosition(), m_Q.Range);
            std::vector<SDK::AIBaseClient> baseList(minions.begin(), minions.end());
            auto farm = m_Q.GetCircularFarmLocation(baseList, 150.0f);
            if (farm.MinionsHit >= FarmMinions()) m_Q.Cast(farm.Position);
        }
    }

    void LogicW() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_W.Range, DamageType::Magical) : AIHeroClient();
        const auto p = Player();

        if (t.IsValid()) {
            const float qDmg = m_Q.GetDamage(t);
            const float wDmg = OktwCommon::GetKsDamage(t, m_W) + BonusDmg(t);
            const Vector3 tpos = p.Position() + (t.Position() - p.Position()).Normalized() * 450.0f;

            if (wDmg > t.Health()) {
                m_W.Cast(tpos);
            } else if (wDmg + qDmg > t.Health() && p.Mana() > m_QMANA + m_EMANA) {
                m_W.Cast(tpos);
            } else if (Combo() && p.Mana() > m_RMANA + m_WMANA) {
                m_W.Cast(tpos);
            } else if (Harass() && GetBool("harassW") &&
                       GetBool((std::string("Harass") + t.CharacterName()).c_str()) &&
                       p.Mana() > m_RMANA + m_WMANA + m_EMANA + m_QMANA + m_WMANA &&
                       OktwCommon::CanHarras()) {
                m_W.Cast(tpos);
            }
        } else if (FarmSpells() && GetBool("farmW")) {
            auto minions = OktwCommon::GetMinions(p.ServerPosition(), m_W.Range);
            std::vector<SDK::AIBaseClient> baseList(minions.begin(), minions.end());
            auto farm = m_W.GetCircularFarmLocation(baseList, m_W.Width);
            if (farm.MinionsHit >= FarmMinions()) m_W.Cast(farm.Position);
        }
    }

    void LogicE() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_E.Range, DamageType::Magical) : AIHeroClient();
        const auto p = Player();

        if (t.IsValid()) {
            const float eDmg = OktwCommon::GetKsDamage(t, m_E) + BonusDmg(t);
            const float wDmg = m_W.GetDamage(t);

            if (eDmg > t.Health()) m_E.Cast(t);
            else if (m_W.IsReady() && wDmg + eDmg > t.Health() && p.Mana() > m_WMANA + m_EMANA) m_E.Cast(t);
            else if (m_R.IsReady() && m_W.IsReady() && wDmg + eDmg + m_R.GetDamage(t) > t.Health() &&
                     p.Mana() > m_WMANA + m_EMANA + m_RMANA) m_E.Cast(t);

            if (Combo() && p.Mana() > m_RMANA + m_EMANA) m_E.Cast(t);
            else if (Harass() && GetBool("harassE") &&
                     p.Mana() > m_RMANA + m_EMANA + m_WMANA + m_EMANA &&
                     GetBool((std::string("Harass") + t.CharacterName()).c_str())) {
                m_E.Cast(t);
            }
        } else if (FarmSpells() && GetBool("farmE")) {
            auto minions = OktwCommon::GetMinions(p.ServerPosition(), m_E.Range);
            if (static_cast<int>(minions.size()) >= FarmMinions()) {
                for (const auto& minion : minions) {
                    if (!SDK::Extensions::IsValidTarget(minion, m_E.Range, true)) continue;
                    if (minion.Health() >= m_E.GetDamage(minion)) continue;
                    if (minion.HasBuff("AlZaharMaleficVisions")) continue;
                    m_E.Cast(minion);
                }
            }
        } else if (Harass() && p.Mana() > m_RMANA + m_EMANA + m_WMANA + m_EMANA &&
                   GetBool("harrasEminion")) {
            auto* ts2 = SDK::TargetSelector::Instance();
            auto te = ts2 ? ts2->GetTarget(m_E.Range + 400.0f, DamageType::Magical) : AIHeroClient();
            if (te.IsValid()) {
                auto minions = OktwCommon::GetMinions(p.ServerPosition(), m_E.Range);
                for (const auto& minion : minions) {
                    if (!SDK::Extensions::IsValidTarget(minion, m_E.Range, true)) continue;
                    if (minion.Health() >= m_E.GetDamage(minion)) continue;
                    if (te.Position().Distance(minion.Position()) >= 500.0f) continue;
                    if (minion.HasBuff("AlZaharMaleficVisions")) continue;
                    m_E.Cast(minion);
                }
            }
        }
    }

    void LogicR() {
        // TODO(oktw-port): UnderTurret check unavailable
        const auto p = Player();
        if (OktwCommon::CountEnemiesInRange(p.Position(), 800.0f) < 3) return;

        for (const auto& t : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!t.IsValid() || !t.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(t, m_R.Range, true)) continue;

            float totalComboDamage = m_R.GetDamage(t) * 2.5f;
            totalComboDamage += m_E.GetDamage(t);

            if (m_W.IsReady() && p.Mana() > m_RMANA + m_WMANA)
                totalComboDamage += m_Q.GetDamage(t);
            if (p.Mana() > m_RMANA + m_QMANA)
                totalComboDamage += m_Q.GetDamage(t);

            if (totalComboDamage > t.Health() - OktwCommon::GetIncomingDamage(t) &&
                OktwCommon::ValidUlt(t)) {
                m_R.Cast(t);
            }
        }
    }

    void Jungle() {
        if (!LaneClear()) return;
        const auto p = Player();
        if (p.Mana() <= m_RMANA + m_EMANA) return;
        auto mobs = OktwCommon::GetMinions(p.ServerPosition(), 600.0f, false, true);
        if (mobs.empty()) return;
        const auto& mob = mobs.front();
        if (m_W.IsReady() && GetBool("jungleW")) { m_W.Cast(mob.ServerPosition()); return; }
        if (m_Q.IsReady() && GetBool("jungleQ")) { m_Q.Cast(mob.ServerPosition()); return; }
        if (m_E.IsReady() && GetBool("jungleE") && mob.HasBuff("brandablaze")) {
            m_E.Cast(mob); return;
        }
    }

    float BonusDmg(const AIHeroClient& target) {
        const auto p = Player();
        const float raw = (target.MaxHealth() * 0.08f) - (0.0f * 5.0f); // TODO(oktw-port): HPRegenRate() not available in SDK
        return p.CalculateMagicDamage(target, raw);
    }

    void OnGameDraw() override {}
};

} } // namespace Plugins::OKTW
