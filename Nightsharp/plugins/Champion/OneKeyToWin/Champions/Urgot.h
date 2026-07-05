#pragma once
// Port of OKTW_CSharp/Champions/Urgot.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;
using SDK::UI::MenuKeyBind;

class UrgotPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Urgot"; }
    const char* GetInternalId() const override { return "champion.oktw.urgot"; }
    const char* GetChampionName() const override { return "Urgot"; }

protected:
    Spell m_Q1{ SpellSlot::Q };  // locked-on Q variant (after E debuff)
    static constexpr int kMuramana = 3042;
    static constexpr int kTear     = 3070;
    static constexpr int kManamune = 3004;

    void BuildMenu() override {
        MarkActive();

        m_Q  = Spell(SpellSlot::Q, 980.0f);
        m_Q1 = Spell(SpellSlot::Q, 1200.0f);
        m_W  = Spell(SpellSlot::W);
        m_E  = Spell(SpellSlot::E, 890.0f);
        m_R  = Spell(SpellSlot::R, 850.0f);

        m_Q.SetSkillshot(0.25f, 60.0f, 1600.0f, true,  SDK::SpellType::Line);
        m_Q1.SetSkillshot(0.25f, 60.0f, 1600.0f, false, SDK::SpellType::Line);
        m_E.SetSkillshot(0.25f, 160.0f, 1500.0f, false, SDK::SpellType::Circle);

        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw only ready spells", true));
        m_drawMenu->Add(new MenuBool("qRange",  "Q range", false));
        m_drawMenu->Add(new MenuBool("eRange",  "E range", false));
        m_drawMenu->Add(new MenuBool("rRange",  "R range", false));

        m_wMenu->Add(new MenuBool("autoW",   "Auto W", true));
        m_wMenu->Add(new MenuBool("Waa",     "Auto W befor AA", true));
        m_wMenu->Add(new MenuBool("AGC",     "AntiGapcloserW", true));
        m_wMenu->Add(new MenuSlider("Wdmg",  "W dmg % hp", 10, 0, 100));

        m_eMenu->Add(new MenuBool("autoE",   "Auto E", true));
        m_eMenu->Add(new MenuBool("harassE", "E harass", true));

        m_rMenu->Add(new MenuBool("autoR",   "Auto R under turrent", true));
        m_rMenu->Add(new MenuBool("inter",   "OnPossibleToInterrupt R", true));
        m_rMenu->Add(new MenuSlider("Rhp",   "dont R if under % hp", 50, 0, 100));
        m_rMenu->Add(new MenuKeyBind("useR", "Semi-manual cast R key", 'T', SDK::KeyBindType::Press));

        Menu* gap = m_rMenu->AddSubMenu(new Menu("GapCloserR", "GapCloser R"));
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string id = std::string("GapCloser") + enemy.CharacterName();
            gap->Add(new MenuBool(id.c_str(), enemy.CharacterName().c_str(), false));
        }

        m_menu->Add(new MenuSlider("HarassMana", "Harass Mana", 30, 0, 100));
        m_menu->Add(new MenuBool("stack",        "Stack Tear if full mana", false));

        m_farmMenu->Add(new MenuBool("farmQ", "Farm Q", true));
        m_farmMenu->Add(new MenuBool("LC",    "LaneClear", true));
        m_farmMenu->Add(new MenuBool("LCP",   "FAST LaneClear", true));

        // TODO(oktw-port): AntiGapcloser.OnEnemyGapcloser event for W/R
        // TODO(oktw-port): Interrupter.OnPossibleToInterrupt event for R
        // TODO(oktw-port): Orbwalking.BeforeAttack event for auto-W before AA + Muramana toggle
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
        const auto p = Player();
        if (!p.IsValid() || p.HasBuff("Recall")) return;

        if (GetKey("useR")) {
            auto* ts = SDK::TargetSelector::Instance();
            auto tr = ts ? ts->GetTarget(m_R.Range, DamageType::Physical) : AIHeroClient();
            if (tr.IsValid()) m_R.Cast(tr);
        }

        if (LagFree(0)) SetMana();

        if (LagFree(1) && m_E.IsReady() && GetBool("autoE")) LogicE();
        if (LagFree(2) && m_W.IsReady() && GetBool("autoW")) LogicW();

        if (m_Q.IsReady()) LogicQ();

        if (LagFree(4) && m_R.IsReady()) LogicR();
    }

    void LogicW() {
        const auto p = Player();
        if (p.Mana() > m_RMANA + m_WMANA) {
            const float dmg = OktwCommon::GetIncomingDamage(p);
            const float shieldValue = 20.0f + m_W.Level() * 40.0f + 0.08f * p.MaxMana() + 0.8f * p.AP();
            const float hpPercentage = p.Health() > 0.0f ? (dmg * 100.0f) / p.Health() : 0.0f;

            if (dmg > shieldValue) {
                m_W.Cast();
            } else if (hpPercentage >= (float)GetSlider("Wdmg", 10)) {
                m_W.Cast();
            } else if (p.Health() - dmg < p.Level() * 10.0f) {
                m_W.Cast();
            }
        }
    }

    void LogicQ2() {
        const auto p = Player();
        if (Farm() && GetBool("farmQ")) {
            farmQ();
        } else if (GetBool("stack") && !p.HasBuff("Recall") &&
                   p.Mana() > p.MaxMana() * 0.95f && None() &&
                   (p.HasItem(kTear) || p.HasItem(kManamune))) {
            m_Q.Cast(p.ServerPosition());
        }
    }

    void LogicQ() {
        const auto p = Player();

        if (!None()) {
            // find the enemy with lowest health that has the urgotcorrosivedebuff (E'd) within Q1 range
            AIHeroClient eTarget;
            float lowest = FLT_MAX;
            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                if (!SDK::Extensions::IsValidTarget(enemy, m_Q1.Range, true)) continue;
                if (!enemy.HasBuff("urgotcorrosivedebuff")) continue;
                if (enemy.Health() < lowest) { lowest = enemy.Health(); eTarget = enemy; }
            }
            if (eTarget.IsValid()) {
                m_Q1.Cast(eTarget.ServerPosition());
                if (m_W.IsReady() &&
                    (p.Mana() > m_WMANA + m_QMANA * 4.0f || m_Q.GetDamage(eTarget) * 3.0f > eTarget.Health()) &&
                    GetBool("autoW")) {
                    m_W.Cast();
                }
                return;
            }
        }

        if (LagFree(1)) {
            if (!OktwCommon::CanMove(p)) return;
            const bool cc = !None() && p.Mana() > m_RMANA + m_QMANA + m_EMANA;
            const bool harass = Harass() && p.ManaPercent() > (float)GetSlider("HarassMana", 30) && OktwCommon::CanHarras();

            if (Combo() && p.Mana() > m_RMANA + m_QMANA) {
                auto* ts = SDK::TargetSelector::Instance();
                auto t = ts ? ts->GetTarget(m_Q.Range, DamageType::Physical) : AIHeroClient();
                if (t.IsValid()) CastSpell(m_Q, t);
            }

            // enemies ordered by lowest health
            std::vector<AIHeroClient> enemies;
            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                if (!SDK::Extensions::IsValidTarget(enemy, m_Q.Range, true)) continue;
                enemies.push_back(enemy);
            }
            std::sort(enemies.begin(), enemies.end(),
                      [](const AIHeroClient& a, const AIHeroClient& b) { return a.Health() < b.Health(); });

            for (const auto& t : enemies) {
                const float qDmg = OktwCommon::GetKsDamage(t, m_Q);
                if (qDmg * 2.0f > t.Health()) {
                    CastSpell(m_Q, t);
                    return;
                }
                if (cc && !OktwCommon::CanMove(t)) m_Q.Cast(t);
                if (harass && GetBool((std::string("Harass") + t.CharacterName()).c_str())) {
                    CastSpell(m_Q, t);
                }
            }
        } else if (LagFree(2)) {
            if (Harass() && p.Mana() > m_QMANA) {
                LogicQ2();
            } else if (GetBool("stack") && !p.HasBuff("Recall") &&
                       p.Mana() > p.MaxMana() * 0.95f && None() &&
                       (p.HasItem(kTear) || p.HasItem(kManamune))) {
                const Vector3 cursor = SDK::Game::CursorPos();
                Vector3 dir = cursor - p.Position();
                const float len = dir.Length();
                Vector3 pos = p.Position();
                if (len > 0.001f) pos = p.Position() + dir * (500.0f / len);
                m_Q.Cast(pos);
            }
        }
    }

    void LogicR() {
        m_R.Range = 400.0f + 150.0f * m_R.Level();
        const auto p = Player();
        // TODO(oktw-port): Player.UnderTurret unavailable — auto R under-turret logic best-effort skipped
        if (p.HealthPercent() < (float)GetSlider("Rhp", 50)) return;
        if (!GetBool("autoR")) return;

        for (const auto& target : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!target.IsValid() || !target.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(target, m_R.Range, true)) continue;
            if (!OktwCommon::ValidUlt(target)) continue;

            if (OktwCommon::CountEnemiesInRange(target.Position(), 700.0f) <
                2 + OktwCommon::CountAlliesInRange(p.Position(), 700.0f)) {
                m_R.Cast(target);
            }
        }
    }

    void LogicE() {
        const float qCd = m_Q.Instance().CooldownExpires() - SDK::Game::Time();

        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_E.Range, DamageType::Physical) : AIHeroClient();
        if (!t.IsValid()) return;

        const auto p = Player();
        const float qDmg = m_Q.GetDamage(t);
        const float eDmg = m_E.GetDamage(t);

        if (eDmg > t.Health()) {
            m_E.Cast(t);
        } else if (eDmg + qDmg > t.Health() && p.Mana() > m_EMANA + m_QMANA) {
            CastSpell(m_E, t);
        } else if (eDmg + 3.0f * qDmg > t.Health() && p.Mana() > m_EMANA + m_QMANA * 3.0f) {
            CastSpell(m_E, t);
        } else if (Combo() && p.Mana() > m_EMANA + m_QMANA * 2.0f && qCd < 0.5f) {
            CastSpell(m_E, t);
        } else if (Harass() && p.Mana() > m_RMANA + m_EMANA + m_QMANA * 5.0f &&
                   GetBool("harassE") &&
                   GetBool((std::string("Harass") + t.CharacterName()).c_str())) {
            CastSpell(m_E, t);
        } else if (!None() && p.Mana() > m_RMANA + m_WMANA + m_EMANA) {
            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                if (!SDK::Extensions::IsValidTarget(enemy, m_E.Range, true)) continue;
                if (!OktwCommon::CanMove(enemy)) m_E.Cast(enemy);
            }
        }
    }

    void farmQ() {
        const auto p = Player();
        if (LaneClear()) {
            auto mobs = OktwCommon::GetMinions(p.ServerPosition(), 800.0f, false, true);
            if (!mobs.empty()) {
                m_Q.Cast(mobs.front().Position());
                return;
            }
        }

        if (!GetBool("farmQ")) return;

        auto minions = OktwCommon::GetMinions(p.ServerPosition(), m_Q.Range);
        const float aaRange = p.AttackRange() + p.BoundingRadius();

        for (const auto& mn : minions) {
            if (mn.Position().Distance(p.Position()) <= aaRange + mn.BoundingRadius()) continue;
            if (mn.Health() < m_Q.GetDamage(mn)) {
                m_Q.Cast(mn.Position());
                return;
            }
        }

        if (GetBool("LC") && LaneClear() && FarmSpells()) {
            const bool LCP = GetBool("LCP");
            for (const auto& mn : minions) {
                if (mn.Position().Distance(p.Position()) > aaRange + mn.BoundingRadius()) continue;
                const float hp = mn.Health();
                const float dmgMinion = mn.GetAutoAttackDamage(mn, false);
                const float qDmg = m_Q.GetDamage(mn);
                if (hp < qDmg) {
                    if (hp > dmgMinion) {
                        m_Q.Cast(mn.Position());
                        return;
                    }
                } else if (LCP) {
                    if (hp > dmgMinion + qDmg) {
                        m_Q.Cast(mn.Position());
                        return;
                    }
                }
            }
        }
    }

    void OnGameDraw() override {}
};

} } // namespace Plugins::OKTW
