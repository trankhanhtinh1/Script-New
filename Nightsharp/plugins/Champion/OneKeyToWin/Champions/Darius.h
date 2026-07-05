#pragma once
// Port of OKTW_CSharp/Champions/Darius.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;
using SDK::UI::MenuKeyBind;

class DariusPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Darius"; }
    const char* GetInternalId() const override { return "champion.oktw.darius"; }
    const char* GetChampionName() const override { return "Darius"; }

protected:
    void BuildMenu() override {
        MarkActive();

        m_Q = Spell(SpellSlot::Q, 400.0f);
        m_W = Spell(SpellSlot::W, 145.0f);
        m_E = Spell(SpellSlot::E, 540.0f);
        m_R = Spell(SpellSlot::R, 460.0f);

        // E: skillshot line (pull). C# used width=100, delay=0.01, speed=float.MaxValue.
        m_E.SetSkillshot(0.01f, 100.0f, FLT_MAX, false, SDK::SpellType::Line);

        // Draw
        m_drawMenu->Add(new MenuBool("qRange",  "Q range", false));
        m_drawMenu->Add(new MenuBool("eRange",  "E range", false));
        m_drawMenu->Add(new MenuBool("rRange",  "R range", false));
        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw when skill rdy", true));

        // Q option
        m_qMenu->Add(new MenuBool("Harass",    "Harass Q", true));
        m_qMenu->Add(new MenuBool("qOutRange", "Auto Q only out range AA", true));

        // E Config: per-enemy Use E on <Champ>
        Menu* eon = m_eMenu->AddSubMenu(new Menu("EonSub", "Use E on"));
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            const std::string id = std::string("Eon") + enemy.CharacterName();
            eon->Add(new MenuBool(id.c_str(), enemy.CharacterName().c_str(), true));
        }

        // R option
        m_rMenu->Add(new MenuBool("autoR",       "Auto R", true));
        // Semi-manual cast R: press 'T'
        m_rMenu->Add(new MenuKeyBind("useR", "Semi-manual cast R key", 'T', SDK::KeyBindType::Press));
        m_rMenu->Add(new MenuBool("autoRbuff",   "Auto R if darius execute multi cast time out", true));
        m_rMenu->Add(new MenuBool("autoRdeath",  "Auto R if darius execute multi cast and under 10 % hp", true));

        // Farm
        m_farmMenu->Add(new MenuBool("farmW", "Farm W", true));
        m_farmMenu->Add(new MenuBool("farmQ", "Farm Q", true));
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
            // C#: RMANA = QMANA - Player.PARRegenRate * Q.Instance.Cooldown
            // TODO(oktw-port): SDK::AIHeroClient::PARRegenRate not available; approximate to 0.
            const float parRegen = 0.0f;
            m_RMANA = m_QMANA - parRegen * m_Q.Instance().Cooldown();
        } else {
            m_RMANA = m_R.Instance().ManaCost();
        }
    }

    void OnGameUpdate() override {
        // Semi-manual R (press-key)
        if (m_R.IsReady() && GetKey("useR")) {
            auto* ts = SDK::TargetSelector::Instance();
            auto targetR = ts ? ts->GetTarget(m_R.Range, SDK::DamageType::True) : AIHeroClient();
            if (targetR.IsValid()) {
                m_R.Cast(targetR, true);
            }
        }

        if (LagFree(0)) {
            SetMana();
        }

        if (LagFree(1) && m_W.IsReady()) LogicW();
        if (LagFree(2) && m_Q.IsReady() && SDK::Orbwalker::CanMove(50) &&
            !Player().Spellbook().IsWindingUp()) {
            LogicQ();
        }
        if (LagFree(3) && m_E.IsReady()) LogicE();
        if (LagFree(4) && m_R.IsReady() && GetBool("autoR")) LogicR();
    }

    // Interrupter equivalent: cast E when a targetable interruptable spell fires.
    // TODO(oktw-port): SDK::Interrupter::OnPossibleToInterrupt hook not wired here;
    // logic mirrored in LogicE gapcloser sweep. Keep placeholder for future wiring.
    void OnInterruptableSpell(const AIHeroClient& unit) {
        if (m_E.IsReady() && SDK::Extensions::IsValidTarget(unit, m_E.Range, true)) {
            m_E.Cast(unit);
        }
    }

    // afterAttack: cast W (auto-attack empowerment) after each AA if mana permits.
    // TODO(oktw-port): SDK::Orbwalker AfterAttack hook not exposed; call this from
    // orbwalker event when available. For now expose method + reachable from OnGameUpdate.
    void AfterAttack(const AIHeroClient& target) {
        if (Player().Mana() < m_RMANA + m_WMANA || !m_W.IsReady()) return;
        if (target.IsValid() && SDK::Extensions::IsValidTarget(target)) {
            m_W.Cast();
        }
    }

    void LogicW() {
        if (Player().Spellbook().IsWindingUp()) return;
        if (!GetBool("farmW") || !Farm()) return;

        auto minions = OktwCommon::GetMinions(Player().Position(), Player().AttackRange());
        int countMinions = 0;
        for (const auto& mn : minions) {
            if (mn.Health() < m_W.GetDamage(mn)) ++countMinions;
        }
        if (countMinions > 0) m_W.Cast();
    }

    void LogicE() {
        const auto p = Player();
        if (p.Mana() <= m_RMANA + m_EMANA) return;

        auto* ts = SDK::TargetSelector::Instance();
        auto target = ts ? ts->GetTarget(m_E.Range, SDK::DamageType::Physical) : AIHeroClient();
        if (!target.IsValid()) return;

        const std::string eonId = std::string("Eon") + target.CharacterName();
        if (!GetBool(eonId.c_str())) return;

        // TODO(oktw-port): UnderTurret(any-turret) has no SDK equivalent (only IsUnderEnemyTurret()); safe fallback false.
        const bool underEnemyTurretOnly = false;
        if (!(underEnemyTurretOnly || Combo())) return;

        if (!SDK::Core::Utils::AutoAttack::InAutoAttackRange(target)) {
            m_E.Cast(target);
        }
    }

    void LogicQ() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_Q.Range, SDK::DamageType::Physical) : AIHeroClient();
        const auto p = Player();

        if (t.IsValid()) {
            const bool outRangeGate = !GetBool("qOutRange") || SDK::Core::Utils::AutoAttack::InAutoAttackRange(t);
            if (outRangeGate) {
                if (p.Mana() > m_RMANA + m_QMANA && Combo()) {
                    m_Q.Cast();
                } else if (Harass() && p.Mana() > m_RMANA + m_QMANA + m_EMANA + m_WMANA &&
                           GetBool("Harass") &&
                           GetBool((std::string("Harass") + t.CharacterName()).c_str())) {
                    m_Q.Cast();
                }
            }

            if (!m_R.IsReady() && OktwCommon::GetKsDamage(t, m_Q) > t.Health()) {
                m_Q.Cast();
            }
        } else if (GetBool("farmQ") && FarmSpells()) {
            auto minionsList = OktwCommon::GetMinions(p.ServerPosition(), m_Q.Range);
            for (const auto& mn : minionsList) {
                if (p.Position().Distance(mn.ServerPosition()) > 300.0f &&
                    mn.Health() < m_Q.GetDamage(mn) * 0.6f) {
                    m_Q.Cast();
                    break;
                }
            }
        }
    }

    void LogicR() {
        auto* ts = SDK::TargetSelector::Instance();
        auto targetR = ts ? ts->GetTarget(m_R.Range, SDK::DamageType::True) : AIHeroClient();

        // Time-out / low-hp auto R while player has execute-multi buff.
        if (targetR.IsValid() && OktwCommon::ValidUlt(targetR) && GetBool("autoRbuff")) {
            const float buffTime = OktwCommon::GetPassiveTime(Player(), "dariusexecutemulticast");
            if (((buffTime < 2.0f) ||
                 (Player().HealthPercent() < 10.0f && GetBool("autoRdeath"))) &&
                buffTime > 0.0f) {
                m_R.Cast(targetR, true);
            }
        }

        // KS: iterate enemies, add hemo-stack scaling.
        for (const auto& target : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!target.IsValid() || !target.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(target, m_R.Range, true)) continue;
            if (!OktwCommon::ValidUlt(target)) continue;

            float dmgR = OktwCommon::GetKsDamage(target, m_R);
            if (target.HasBuff("dariushemo")) {
                const int stacks = target.GetBuffCount("dariushemo");
                dmgR += m_R.GetDamage(target) * static_cast<float>(stacks) * 0.2f;
            }

            if (dmgR > target.Health()) {
                m_R.Cast(target);
            }
        }
    }

    void OnGameDraw() override {
        // Simplified: rely on SDK draw utilities for range circles when wired.
        // TODO(oktw-port): draw Q/E/R ranges gated by "onlyRdy"/"qRange"/"eRange"/"rRange".
    }
};

} } // namespace Plugins::OKTW
