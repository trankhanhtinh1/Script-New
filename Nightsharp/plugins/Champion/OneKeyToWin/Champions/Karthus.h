#pragma once
// Port of OKTW_CSharp/Champions/Karthus.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;

class KarthusPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Karthus"; }
    const char* GetInternalId() const override { return "champion.oktw.karthus"; }
    const char* GetChampionName() const override { return "Karthus"; }

protected:
    void BuildMenu() override {
        MarkActive();

        m_Q = Spell(SpellSlot::Q, 890.0f);
        m_W = Spell(SpellSlot::W, 1000.0f);
        m_E = Spell(SpellSlot::E, 520.0f);
        m_R = Spell(SpellSlot::R, 20000.0f);

        m_Q.SetSkillshot(0.95f, 140.0f, FLT_MAX, false, SDK::SpellType::Circle);
        m_W.SetSkillshot(0.50f,  50.0f, FLT_MAX, false, SDK::SpellType::Circle);

        // R.DamageType = TargetSelector.DamageType.Magical; // TODO(oktw-port): SDK Spell has no DamageType field

        m_drawMenu->Add(new MenuBool("noti",    "Show R notification", true));
        m_drawMenu->Add(new MenuBool("qRange",  "Q range", false));
        m_drawMenu->Add(new MenuBool("wRange",  "W range", false));
        m_drawMenu->Add(new MenuBool("eRange",  "E range", false));
        m_drawMenu->Add(new MenuBool("rRange",  "R range", false));
        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw when skill rdy", true));

        m_qMenu->Add(new MenuBool("autoQ",       "Auto Q",     true));
        m_qMenu->Add(new MenuBool("harassQ",     "Harass Q",   true));
        m_qMenu->Add(new MenuSlider("QHarassMana", "Harass Mana", 30, 0, 100));

        Menu* qon = m_qMenu->AddSubMenu(new Menu("QonSub", "Use on:"));
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string id = std::string("Qon") + enemy.CharacterName();
            qon->Add(new MenuBool(id.c_str(), enemy.CharacterName().c_str(), true));
        }

        m_wMenu->Add(new MenuBool("autoW",   "Auto W",   true));
        m_wMenu->Add(new MenuBool("harassW", "Harass W", false));
        static const char* wComboModes[] = { "always", "run - cheese" };
        m_wMenu->Add(new MenuList("WmodeCombo", "W combo mode", wComboModes, 2, 1));

        Menu* wgc = m_wMenu->AddSubMenu(new Menu("WGCSub", "W Gap Closer"));
        static const char* wgcModes[] = { "Dash end position", "My hero position" };
        wgc->Add(new MenuList("WmodeGC", "Gap Closer position mode", wgcModes, 2, 0));
        Menu* wgcOn = wgc->AddSubMenu(new Menu("WGConSub", "Cast on enemy:"));
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string id = std::string("WGCchampion") + enemy.CharacterName();
            wgcOn->Add(new MenuBool(id.c_str(), enemy.CharacterName().c_str(), true));
        }

        m_eMenu->Add(new MenuBool("autoE",  "Auto E if enemy in range", true));
        m_eMenu->Add(new MenuSlider("Emana", "E % minimum mana", 20, 0, 100));

        m_rMenu->Add(new MenuBool("autoR",       "Auto R", true));
        m_rMenu->Add(new MenuBool("autoRzombie", "Auto R upon dying if can help team", true));
        m_rMenu->Add(new MenuSlider("Renemy",    "Don't R if enemy in x range", 1500, 0, 2000));
        m_rMenu->Add(new MenuSlider("RenemyA",   "Don't R if ally in x range near target", 800, 0, 2000));
        m_rMenu->Add(new MenuBool("Rturrent",    "Don't R under turret", true));

        m_farmMenu->Add(new MenuBool("farmQout",    "Last hit Q minion out range AA", true));
        m_farmMenu->Add(new MenuBool("farmQ",       "Lane clear Q", true));
        m_farmMenu->Add(new MenuBool("farmE",       "Lane clear E", true));
        m_farmMenu->Add(new MenuSlider("QLCminions", "QLaneClear minimum minions", 2, 0, 10));
        m_farmMenu->Add(new MenuSlider("ELCminions", "ELaneClear minimum minions", 5, 0, 10));
        m_farmMenu->Add(new MenuBool("jungleQ",     "Jungle clear Q", true));
        m_farmMenu->Add(new MenuBool("jungleE",     "Jungle clear E", true));

        m_champMenu->Add(new MenuBool("autoZombie", "Auto zombie mode COMBO / LANECLEAR", true));

        // TODO(oktw-port): AntiGapcloser hook (W on gapcloser)
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
        if (!m_R.IsReady()) {
            // PARRegenRate approximation: RMANA = QMANA - regen*cooldown
            m_RMANA = m_QMANA; // TODO(oktw-port): PARRegenRate/cooldown unavailable
        } else {
            m_RMANA = m_R.Instance().ManaCost();
        }
    }

    void OnGameUpdate() override {
        const auto p = Player();
        if (!p.IsValid() || p.HasBuff("Recall")) return;

        if (p.IsZombie()) {
            // TODO(oktw-port): Orbwalker::SetActiveMode unavailable — zombie-mode
            // orbwalk override (Combo/LaneClear) skipped.
            if (GetBool("autoZombie")) {
            }
            if (m_R.IsReady() && GetBool("autoRzombie")) {
                float timeDeadh = OktwCommon::GetPassiveTime(p, "KarthusDeathDefiedBuff");
                if (timeDeadh < 4.0f) {
                    for (const auto& target : SDK::ObjectManager::Get<AIHeroClient>()) {
                        if (!target.IsValid() || !target.IsEnemy()) continue;
                        if (!SDK::Extensions::IsValidTarget(target, FLT_MAX, true)) continue;
                        if (!OktwCommon::ValidUlt(target)) continue;
                        const float rDamage = m_R.GetDamage(target);
                        if (target.Health() < 3.0f * rDamage &&
                            OktwCommon::CountAlliesInRange(target.Position(), 800.0f) > 0)
                            m_R.Cast();
                        if (target.Health() < rDamage * 1.5f &&
                            target.Position().Distance(p.Position()) < 900.0f)
                            m_R.Cast();
                        if (target.Health() + 0.0f * 5.0f < rDamage) // TODO(oktw-port): HpRegenRate() not available in SDK
                            m_R.Cast();
                    }
                }
            }
        } else {
            // TODO(oktw-port): Orbwalker::SetActiveMode unavailable — mode reset skipped.
        }

        if (LagFree(0)) { SetMana(); Jungle(); }
        if (LagFree(1) && m_Q.IsReady() && GetBool("autoQ")) LogicQ();
        if (LagFree(2) && m_E.IsReady() && GetBool("autoE")) LogicE();
        if (LagFree(3) && m_R.IsReady()) LogicR();
        if (LagFree(4) && m_W.IsReady() && GetBool("autoW")) LogicW();
    }

    void LogicR() {
        const auto p = Player();
        if (GetBool("autoR") &&
            p.CountEnemyHeroesInRange(static_cast<float>(GetSlider("Renemy", 1500))) == 0) {
            if (p.IsUnderEnemyTurret() && GetBool("Rturrent")) return;

            for (const auto& target : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!target.IsValid() || target.IsDead() || !target.IsEnemy()) continue;
                if (SDK::Extensions::IsValidTarget(target, FLT_MAX, true) &&
                    OktwCommon::CountAlliesInRange(target.Position(),
                        static_cast<float>(GetSlider("RenemyA", 800))) == 0) {
                    const float predictedHealth = target.Health() + 0.0f * 4.0f; // TODO(oktw-port): HpRegenRate() not available in SDK
                    float Rdmg = OktwCommon::GetKsDamage(target, m_R);
                    if (target.HealthPercent() > 30.0f) {
                        if (target.HasItem(3155)) Rdmg -= 250.0f;
                        if (target.HasItem(3156)) Rdmg -= 400.0f;
                    }
                    if (Rdmg > predictedHealth && OktwCommon::ValidUlt(target)) {
                        m_R.Cast();
                    }
                } else if (!target.IsVisible()) {
                    // TODO(oktw-port): Core.OKTWtracker.ChampionInfoList — invisibility tracker unavailable
                }
            }
        }
    }

    float GetQDamage(const SDK::AIBaseClient& t) {
        auto minions = OktwCommon::GetMinions(t.Position(), m_Q.Width + 20.0f);
        if (static_cast<int>(minions.size()) > 1)
            return m_Q.GetDamage(t, SDK::DamageStage::SecondForm);
        else
            return m_Q.GetDamage(t);
    }

    void LogicQ() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_Q.Range, DamageType::Magical) : AIHeroClient();
        const auto p = Player();
        if (t.IsValid() && GetBool((std::string("Qon") + t.CharacterName()).c_str())) {
            if (Combo() && p.Mana() > m_RMANA + m_QMANA + m_WMANA) {
                CastSpell(m_Q, t);
            } else if (Harass() && OktwCommon::CanHarras() &&
                       GetBool("harassQ") &&
                       GetBool((std::string("Harass") + t.CharacterName()).c_str()) &&
                       p.ManaPercent() > static_cast<float>(GetSlider("QHarassMana", 30))) {
                CastSpell(m_Q, t);
            } else if (OktwCommon::GetKsDamage(t, m_Q) > t.Health()) {
                CastSpell(m_Q, t);
            }

            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (enemy.IsValid() && enemy.IsEnemy() &&
                    SDK::Extensions::IsValidTarget(enemy, m_Q.Range, true) &&
                    !OktwCommon::CanMove(enemy)) {
                    CastSpell(m_Q, t);
                }
            }
        }
        if (!OktwCommon::CanHarras()) return;

        if (!None() && !Combo() && p.Mana() > m_RMANA + m_QMANA * 2.0f) {
            auto allMinions = OktwCommon::GetMinions(p.ServerPosition(), m_Q.Range);

            if (GetBool("farmQout")) {
                for (const auto& minion : allMinions) {
                    if (!SDK::Extensions::IsValidTarget(minion, m_Q.Range, true)) continue;
                    const bool inAA = SDK::Core::Utils::AutoAttack::InAutoAttackRange(minion);
                    const bool underTurretOK = !minion.IsUnderEnemyTurret() && minion.IsUnderAllyTurret();
                    if (!inAA || underTurretOK) {
                        const float hpPred = SDK::HealthPrediction::GetPrediction(minion, 1100, 0);
                        if (hpPred < GetQDamage(minion) * 0.9f &&
                            hpPred > minion.Health() - hpPred * 2.0f) {
                            m_Q.Cast(minion);
                            return;
                        }
                    }
                }
            }

            if (GetBool("farmQ") && FarmSpells()) {
                for (const auto& minion : allMinions) {
                    if (!SDK::Extensions::IsValidTarget(minion, m_Q.Range, true)) continue;
                    if (!SDK::Core::Utils::AutoAttack::InAutoAttackRange(minion)) continue;
                    const float hpPred = SDK::HealthPrediction::GetPrediction(minion, 1100, 0);
                    if (hpPred < GetQDamage(minion) * 0.9f &&
                        hpPred > minion.Health() - hpPred * 2.0f) {
                        m_Q.Cast(minion);
                        return;
                    }
                }
                std::vector<SDK::AIBaseClient> baseList(allMinions.begin(), allMinions.end());
                auto farmPos = m_Q.GetCircularFarmLocation(baseList, m_Q.Width);
                if (farmPos.MinionsHit >= GetSlider("QLCminions", 2))
                    m_Q.Cast(farmPos.Position);
            }
        }
    }

    void LogicW() {
        const auto p = Player();
        if ((Combo() || (Harass() && GetBool("harassW"))) && p.Mana() > m_RMANA + m_WMANA) {
            if (GetList("WmodeCombo", 1) == 1) {
                auto* ts = SDK::TargetSelector::Instance();
                auto t = ts ? ts->GetTarget(m_W.Range, DamageType::Magical) : AIHeroClient();
                if (SDK::Extensions::IsValidTarget(t, m_W.Range, true) &&
                    m_W.GetPrediction(t).GetCastPosition().Distance(t.Position()) > 100.0f) {
                    if (p.Position().Distance(t.ServerPosition()) > p.Position().Distance(t.Position())) {
                        if (t.Position().Distance(p.ServerPosition()) < t.Position().Distance(p.Position()))
                            CastSpell(m_W, t);
                    } else {
                        if (t.Position().Distance(p.ServerPosition()) > t.Position().Distance(p.Position()))
                            CastSpell(m_W, t);
                    }
                }
            } else {
                auto* ts = SDK::TargetSelector::Instance();
                auto t = ts ? ts->GetTarget(m_W.Range, DamageType::Magical) : AIHeroClient();
                if (t.IsValid()) CastSpell(m_W, t);
            }
        }
    }

    void LogicE() {
        if (None()) return;
        const auto p = Player();

        if (p.HasBuff("KarthusDefile")) {
            if (LaneClear()) {
                const int mana = m_manaSlider ? m_manaSlider->Value : 50;
                if (OktwCommon::CountEnemyMinions(p.Position(), m_E.Range) <
                        GetSlider("ELCminions", 5) ||
                    p.ManaPercent() < static_cast<float>(mana))
                    m_E.Cast();
            } else if (GetBool("autoE")) {
                if (p.ManaPercent() < static_cast<float>(GetSlider("Emana", 20)) ||
                    p.CountEnemyHeroesInRange(m_E.Range) == 0)
                    m_E.Cast();
            }
        } else {
            if (LaneClear()) {
                if (OktwCommon::CountEnemyMinions(p.Position(), m_E.Range) >=
                        GetSlider("ELCminions", 5) && FarmSpells())
                    m_E.Cast();
            } else if (GetBool("autoE") &&
                       p.ManaPercent() > static_cast<float>(GetSlider("Emana", 20)) &&
                       p.CountEnemyHeroesInRange(m_E.Range) > 0) {
                m_E.Cast();
            }
        }
    }

    void Jungle() {
        const auto p = Player();
        if (LaneClear() && p.Mana() > m_RMANA + m_QMANA) {
            auto mobs = OktwCommon::GetMinions(p.ServerPosition(), m_Q.Range, false, true);
            if (mobs.empty()) return;
            const auto& mob = mobs.front();
            if (m_Q.IsReady() && GetBool("jungleQ")) {
                m_Q.Cast(mob.ServerPosition());
                return;
            }
            if (m_E.IsReady() && GetBool("jungleE") &&
                SDK::Extensions::IsValidTarget(mob, m_E.Range, true)) {
                m_E.Cast(mob.ServerPosition());
                return;
            }
        }
    }

    void OnGameDraw() override {
        // Simplified: rely on SDK draw utilities for range circles
    }
};

} } // namespace Plugins::OKTW
