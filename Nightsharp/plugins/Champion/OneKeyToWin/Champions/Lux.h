#pragma once
// Port of OKTW_CSharp/Champions/Lux.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;
using SDK::UI::MenuKeyBind;

class LuxPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Lux"; }
    const char* GetInternalId() const override { return "champion.oktw.lux"; }
    const char* GetChampionName() const override { return "Lux"; }

protected:
    Spell m_Q1{ SpellSlot::Q };
    Vector3 m_Epos{};
    float   m_dragonDmg = 0.0f;
    double  m_dragonTime = 0.0;

    void BuildMenu() override {
        MarkActive();

        m_Q  = Spell(SpellSlot::Q, 1175.0f);
        m_Q1 = Spell(SpellSlot::Q, 1175.0f);
        m_W  = Spell(SpellSlot::W, 1075.0f);
        m_E  = Spell(SpellSlot::E, 1075.0f);
        m_R  = Spell(SpellSlot::R, 3000.0f);

        m_Q1.SetSkillshot(0.25f, 80.0f,  1200.0f,      true,  SDK::SpellType::Line);
        m_Q.SetSkillshot (0.25f, 80.0f,  1200.0f,      false, SDK::SpellType::Line);
        m_W.SetSkillshot (0.25f, 110.0f, 1200.0f,      false, SDK::SpellType::Line);
        m_E.SetSkillshot (0.30f, 250.0f, 1050.0f,      false, SDK::SpellType::Circle);
        m_R.SetSkillshot (1.35f, 190.0f, FLT_MAX,      false, SDK::SpellType::Line);

        m_drawMenu->Add(new MenuBool("noti",       "Show notification",       true));
        m_drawMenu->Add(new MenuBool("qRange",     "Q range",                 false));
        m_drawMenu->Add(new MenuBool("wRange",     "W range",                 false));
        m_drawMenu->Add(new MenuBool("eRange",     "E range",                 false));
        m_drawMenu->Add(new MenuBool("rRange",     "R range",                 false));
        m_drawMenu->Add(new MenuBool("rRangeMini", "R range minimap",         true));
        m_drawMenu->Add(new MenuBool("onlyRdy",    "Draw when skill rdy",     true));

        m_qMenu->Add(new MenuBool("autoQ",   "Auto Q",   true));
        m_qMenu->Add(new MenuBool("harassQ", "Harass Q", true));

        Menu* qgap = m_qMenu->AddSubMenu(new Menu("QGapSub", "Q Gap Closer"));
        qgap->Add(new MenuBool("gapQ", "Auto Q Gap Closer", true));
        Menu* qgapOn = qgap->AddSubMenu(new Menu("QGapOnSub", "Use on:"));
        Menu* qOn    = m_qMenu->AddSubMenu(new Menu("QOnSub",    "Use on:"));
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string idG = std::string("Qgap") + enemy.CharacterName();
            const std::string idO = std::string("Qon")  + enemy.CharacterName();
            qgapOn->Add(new MenuBool(idG.c_str(), enemy.CharacterName().c_str(), true));
            qOn->Add   (new MenuBool(idO.c_str(), enemy.CharacterName().c_str(), true));
        }

        m_eMenu->Add(new MenuBool("autoE",     "Auto E",                          true));
        m_eMenu->Add(new MenuBool("harassE",   "Harass E",                        false));
        m_eMenu->Add(new MenuBool("autoEcc",   "Auto E only CC enemy",            false));
        m_eMenu->Add(new MenuBool("autoEslow", "Auto E slow logic detonate",      true));
        m_eMenu->Add(new MenuBool("autoEdet",  "Only detonate if target in E",    false));

        m_wMenu->Add(new MenuSlider("Wdmg", "W dmg % hp", 10, 0, 100));
        Menu* shieldAlly = m_wMenu->AddSubMenu(new Menu("WShieldAlly", "Shield ally"));
        for (const auto& ally : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!ally.IsValid() || ally.IsEnemy()) continue;
            Menu* am = shieldAlly->AddSubMenu(new Menu(
                (std::string("WAlly") + ally.CharacterName()).c_str(),
                ally.CharacterName().c_str()));
            am->Add(new MenuBool((std::string("damage") + ally.CharacterName()).c_str(), "Damage incoming", true));
            am->Add(new MenuBool((std::string("HardCC") + ally.CharacterName()).c_str(), "Hard CC", true));
            am->Add(new MenuBool((std::string("Poison") + ally.CharacterName()).c_str(), "Poison", true));
        }

        m_rMenu->Add(new MenuBool  ("autoR",      "Auto R", true));
        m_rMenu->Add(new MenuBool  ("passiveR",   "Include R passive damage", false));
        m_rMenu->Add(new MenuBool  ("Rcc",        "R fast KS combo", true));
        m_rMenu->Add(new MenuSlider("RaoeCount",  "R x enemies in combo [0 == off]", 3, 0, 5));
        m_rMenu->Add(new MenuSlider("hitchanceR", "Hit Chance R", 2, 0, 3));
        m_rMenu->Add(new MenuKeyBind("useR",      "Semi-manual cast R key", 'T', SDK::KeyBindType::Press));

        Menu* rjungle = m_rMenu->AddSubMenu(new Menu("RJungleSub", "R Jungle stealer"));
        rjungle->Add(new MenuBool("Rjungle", "R Jungle stealer", true));
        rjungle->Add(new MenuBool("Rdragon", "Dragon",           true));
        rjungle->Add(new MenuBool("Rbaron",  "Baron",            true));
        rjungle->Add(new MenuBool("Rred",    "Red",              true));
        rjungle->Add(new MenuBool("Rblue",   "Blue",             true));
        rjungle->Add(new MenuBool("Rally",   "Ally stealer",     false));

        m_farmMenu->Add(new MenuBool("farmE",    "Lane clear E",   true));
        m_farmMenu->Add(new MenuBool("jungleQ",  "Jungle clear Q", true));
        m_farmMenu->Add(new MenuBool("jungleE",  "Jungle clear E", true));
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
        if (m_R.IsReady()) {
            if (GetBool("Rjungle")) KsJungle();
            if (GetKey("useR")) {
                auto* ts = SDK::TargetSelector::Instance();
                auto t = ts ? ts->GetTarget(m_R.Range, DamageType::Magical) : AIHeroClient();
                if (t.IsValid()) m_R.Cast(t);
            }
        } else {
            m_dragonTime = 0.0;
        }

        if (LagFree(0)) { SetMana(); Jungle(); }

        if ((LagFree(4) || LagFree(1) || LagFree(3)) && m_W.IsReady())
            LogicW();
        if (LagFree(1) && m_Q.IsReady() && GetBool("autoQ")) LogicQ();
        if (LagFree(2) && m_E.IsReady() && GetBool("autoE")) LogicE();
        if (LagFree(3) && m_R.IsReady()) LogicR();
    }

    static bool HardCC(const AIHeroClient& target) {
        using namespace SDK::Prediction::BuffType;
        return OktwCommon::HasBuffOfType(target, Stun) ||
               OktwCommon::HasBuffOfType(target, Snare) ||
               OktwCommon::HasBuffOfType(target, Knockup) ||
               OktwCommon::HasBuffOfType(target, Charm) ||
               OktwCommon::HasBuffOfType(target, Fear) ||
               OktwCommon::HasBuffOfType(target, Knockback) ||
               OktwCommon::HasBuffOfType(target, Taunt) ||
               OktwCommon::HasBuffOfType(target, Suppression);
    }

    void LogicW() {
        const auto p = Player();
        for (const auto& ally : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!ally.IsValid() || ally.IsEnemy() || ally.IsDead()) continue;
            const std::string dmgId = std::string("damage") + ally.CharacterName();
            if (!GetBool(dmgId.c_str())) continue;
            if (p.ServerPosition().Distance(ally.ServerPosition()) >= m_W.Range) continue;

            float dmg = OktwCommon::GetIncomingDamage(ally);
            int nearEnemys = OktwCommon::CountEnemiesInRange(ally.Position(), 800.0f);

            if (dmg == 0.0f && nearEnemys == 0) continue;

            const int sensitivity = 20;
            const float hpPercentage = (dmg * 100.0f) / std::max(ally.Health(), 1.0f);
            const float shieldValue = 65.0f + static_cast<float>(m_W.Level()) * 25.0f + 0.35f * p.AP();

            const std::string ccId = std::string("HardCC") + ally.CharacterName();
            const std::string poisonId = std::string("Poison") + ally.CharacterName();

            if (GetBool(ccId.c_str()) && nearEnemys > 0 && HardCC(ally)) {
                m_W.Cast(ally);
                continue;
            } else if (GetBool(poisonId.c_str()) && false /* TODO(oktw-port): BuffType::Poison unavailable */) {
                m_W.Cast(m_W.GetPrediction(ally, true).GetCastPosition());
            }

            if (nearEnemys == 0) nearEnemys = 1;

            if (dmg > shieldValue) {
                m_W.Cast(m_W.GetPrediction(ally, true).GetCastPosition());
            } else if (dmg > 100.0f + static_cast<float>(p.Level()) * sensitivity) {
                m_W.Cast(m_W.GetPrediction(ally, true).GetCastPosition());
            } else if (ally.Health() - dmg < nearEnemys * ally.Level() * sensitivity) {
                m_W.Cast(m_W.GetPrediction(ally, true).GetCastPosition());
            } else if (hpPercentage >= static_cast<float>(GetSlider("Wdmg", 10))) {
                m_W.Cast(m_W.GetPrediction(ally, true).GetCastPosition());
            }
        }
    }

    void CastQ(const AIHeroClient& t) {
        const auto p = m_Q1.GetPrediction(t, true);
        // Collision object filter: skip if 4+ minions in the way
        if (static_cast<int>(p.CollisionObjects.size()) < 4) CastSpell(m_Q, t);
    }

    void LogicQ() {
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(enemy, m_Q.Range, true)) continue;
            if (m_E.GetDamage(enemy) + m_Q.GetDamage(enemy) + BonusDmg(enemy) > enemy.Health()) {
                CastQ(enemy);
                return;
            }
        }

        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_Q.Range, DamageType::Magical) : AIHeroClient();
        const auto p = Player();
        if (t.IsValid()) {
            const std::string qonId = std::string("Qon") + t.CharacterName();
            if (!GetBool(qonId.c_str())) return;

            if (Combo() && p.Mana() > m_RMANA + m_QMANA) {
                CastQ(t);
            } else if (Harass() && GetBool("harassQ") &&
                       GetBool((std::string("Harass") + t.CharacterName()).c_str()) &&
                       p.Mana() > m_RMANA + m_EMANA + m_WMANA + m_EMANA) {
                CastQ(t);
            } else if (OktwCommon::GetKsDamage(t, m_Q) > t.Health()) {
                CastQ(t);
            }

            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                if (SDK::Extensions::IsValidTarget(enemy, m_Q.Range, true) &&
                    !OktwCommon::CanMove(enemy)) {
                    CastQ(enemy);
                }
            }
        }
    }

    void LogicE() {
        const auto p = Player();
        if (p.HasBuff("LuxLightStrikeKugel") && !None()) {
            int eBig = OktwCommon::CountEnemiesInRange(m_Epos, 350.0f);
            if (GetBool("autoEslow")) {
                int detonate = eBig - OktwCommon::CountEnemiesInRange(m_Epos, 160.0f);
                if (detonate > 0 || eBig > 1) m_E.Cast();
            } else if (GetBool("autoEdet")) {
                if (eBig > 0) m_E.Cast();
            } else {
                m_E.Cast();
            }
            return;
        }

        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_E.Range, DamageType::Magical) : AIHeroClient();
        if (t.IsValid()) {
            if (!GetBool("autoEcc")) {
                if (Combo() && p.Mana() > m_RMANA + m_EMANA) {
                    CastSpell(m_E, t);
                } else if (Harass() && OktwCommon::CanHarras() &&
                           GetBool((std::string("Harass") + t.CharacterName()).c_str()) &&
                           GetBool("harassE") &&
                           p.Mana() > m_RMANA + m_EMANA + m_EMANA + m_RMANA) {
                    CastSpell(m_E, t);
                } else if (OktwCommon::GetKsDamage(t, m_E) > t.Health()) {
                    CastSpell(m_E, t);
                }
            }
            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                if (SDK::Extensions::IsValidTarget(enemy, m_E.Range, true) &&
                    !OktwCommon::CanMove(enemy)) {
                    m_E.Cast(enemy.Position());
                }
            }
        } else if (FarmSpells() && GetBool("farmE") && p.Mana() > m_RMANA + m_WMANA) {
            auto minions = OktwCommon::GetMinions(p.ServerPosition(), m_E.Range);
            std::vector<SDK::AIBaseClient> baseList(minions.begin(), minions.end());
            auto farm = m_E.GetCircularFarmLocation(baseList, m_E.Width);
            if (farm.MinionsHit >= FarmMinions()) m_E.Cast(farm.Position);
        }
    }

    void CastR(const AIHeroClient& target) {
        const int inx = GetSlider("hitchanceR", 2);
        if (inx == 0) {
            m_R.Cast(m_R.GetPrediction(target, true).GetCastPosition());
        } else if (inx == 1) {
            m_R.Cast(target);
        } else if (inx == 2) {
            CastSpell(m_R, target);
        } else if (inx == 3) {
            // Waypoint prediction not exposed — fallback to CastSpell
            CastSpell(m_R, target);
        }
    }

    void LogicR() {
        if (!GetBool("autoR")) return;
        const auto p = Player();
        for (const auto& target : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!target.IsValid() || !target.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(target, m_R.Range, true)) continue;
            if (OktwCommon::CountAlliesInRange(target.Position(), 600.0f) >= 2) continue;
            if (!OktwCommon::ValidUlt(target)) continue;

            float rDmg = OktwCommon::GetKsDamage(target, m_R);
            if (target.HasItem(3155)) rDmg -= 250.0f;
            if (target.HasItem(3156)) rDmg -= 400.0f;

            if (GetBool("passiveR")) {
                if (target.HasBuff("luxilluminatingfraulein")) {
                    rDmg += p.CalculateMagicDamage(target, 10.0f + (8.0f * p.Level()) + 0.2f * p.AP());
                }
                if (p.HasBuff("itemmagicshankcharge")) {
                    // Buff stack check deferred — approximate contribution
                    rDmg += p.CalculateMagicDamage(target, 100.0f + 0.1f * p.AP());
                }
            }

            if (rDmg > target.Health()) {
                CastR(target);
            } else if (!OktwCommon::CanMove(target) && GetBool("Rcc") &&
                       SDK::Extensions::IsValidTarget(target, m_E.Range, true)) {
                float dmgCombo = rDmg;
                if (m_E.IsReady()) {
                    const float eDmg = m_E.GetDamage(target);
                    if (eDmg > target.Health()) return;
                    dmgCombo += eDmg;
                }
                if (SDK::Extensions::IsValidTarget(target, 800.0f, true))
                    dmgCombo += BonusDmg(target);
                if (dmgCombo > target.Health()) {
                    auto pr = m_R.GetPrediction(target, true);
                    if (pr.AoeTargetsHitCount >= 2) m_R.Cast(pr.GetCastPosition());
                    m_R.Cast(target);
                }
            } else if (Combo() && GetSlider("RaoeCount", 3) > 0) {
                auto pr = m_R.GetPrediction(target, true);
                if (pr.AoeTargetsHitCount >= GetSlider("RaoeCount", 3))
                    m_R.Cast(pr.GetCastPosition());
            }
        }
    }

    float BonusDmg(const AIHeroClient& target) {
        const auto p = Player();
        float damage = 10.0f + (p.Level()) * 8.0f + 0.2f * p.AP();
        if (p.HasBuff("lichbane")) {
            damage += (p.BaseAttackDamage() * 0.75f) +
                      (p.AP() * 0.5f); // TODO(oktw-port): BaseAbilityDamage/FlatMagicDamageMod folded into AP()
        }
        return p.GetAutoAttackDamage(target, false) + p.CalculateMagicDamage(target, damage);
    }

    void Jungle() {
        if (!LaneClear()) return;
        const auto p = Player();
        if (p.Mana() <= m_RMANA + m_WMANA + m_RMANA + m_WMANA) return;
        auto mobs = OktwCommon::GetMinions(p.ServerPosition(), 600.0f, false, true);
        if (mobs.empty()) return;
        const auto& mob = mobs.front();
        if (m_Q.IsReady() && GetBool("jungleQ")) { m_Q.Cast(mob.ServerPosition()); return; }
        if (m_E.IsReady() && GetBool("jungleE")) { m_E.Cast(mob.ServerPosition()); return; }
    }

    void KsJungle() {
        const auto p = Player();
        auto mobs = OktwCommon::GetMinions(p.ServerPosition(), m_R.Range, false, true);
        for (const auto& mob : mobs) {
            const std::string& skin = mob.CharacterName();
            bool typeOk =
                (skin == "SRU_Dragon" && GetBool("Rdragon")) ||
                (skin == "SRU_Baron"  && GetBool("Rbaron"))  ||
                (skin == "SRU_Red"    && GetBool("Rred"))    ||
                (skin == "SRU_Blue"   && GetBool("Rblue"));
            if (!typeOk) continue;

            const bool alliesFar = OktwCommon::CountAlliesInRange(mob.Position(), 1000.0f) == 0;
            if (!(alliesFar || GetBool("Rally"))) continue;
            if (mob.Health() >= mob.MaxHealth()) continue;
            if (mob.Position().Distance(p.Position()) <= 1000.0f) continue;

            if (m_dragonDmg == 0.0f) m_dragonDmg = mob.Health();

            const double now = SDK::Game::Time();
            if (now - m_dragonTime > 3.0) {
                if (m_dragonDmg - mob.Health() > 0.0f) m_dragonDmg = mob.Health();
                m_dragonTime = now;
            } else {
                const double dmgSec = (m_dragonDmg - mob.Health()) * (std::abs(m_dragonTime - now) / 3.0);
                if (m_dragonDmg - mob.Health() > 0.0f) {
                    const float timeTravel = m_R.Delay;
                    const double timeR = (mob.Health() - m_R.GetDamage(mob)) / (dmgSec / 3.0);
                    if (timeTravel > timeR) m_R.Cast(mob.Position());
                } else {
                    m_dragonDmg = mob.Health();
                }
            }
        }
    }

    void OnGameDraw() override {}
};

} } // namespace Plugins::OKTW
