#pragma once
// Port of OKTW_CSharp/Champions/Lucian.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;
using SDK::UI::MenuKeyBind;

class LucianPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Lucian"; }
    const char* GetInternalId() const override { return "champion.oktw.lucian"; }
    const char* GetChampionName() const override { return "Lucian"; }

protected:
    // C# had Q (targetted 675) + Q1 (skillshot line 900), and R + R1 (with/without collision)
    Spell m_Q1{ SpellSlot::Q };
    Spell m_R1{ SpellSlot::R };
    bool  m_passRdy = false;
    float m_castR   = 0.0f;

    void BuildMenu() override {
        MarkActive();

        m_Q  = Spell(SpellSlot::Q, 675.0f);
        m_Q1 = Spell(SpellSlot::Q, 900.0f);
        m_W  = Spell(SpellSlot::W, 1100.0f);
        m_E  = Spell(SpellSlot::E, 475.0f);
        m_R  = Spell(SpellSlot::R, 1200.0f);
        m_R1 = Spell(SpellSlot::R, 1200.0f);

        m_Q1.SetSkillshot(0.40f, 10.0f, FLT_MAX, true, SDK::SpellType::Line);
        m_Q.SetTargetted(0.25f, 1400.0f);
        m_W.SetSkillshot(0.30f, 80.0f, 1600.0f, true, SDK::SpellType::Line);
        m_R.SetSkillshot(0.10f, 110.0f, 2800.0f, true,  SDK::SpellType::Line);
        m_R1.SetSkillshot(0.10f, 110.0f, 2800.0f, false, SDK::SpellType::Line);

        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw only ready spells", true));
        m_drawMenu->Add(new MenuBool("qRange",  "Q range", false));
        m_drawMenu->Add(new MenuBool("wRange",  "W range", false));
        m_drawMenu->Add(new MenuBool("eRange",  "E range", false));
        m_drawMenu->Add(new MenuBool("rRange",  "R range", false));

        m_qMenu->Add(new MenuBool("autoQ",   "Auto Q", true));
        m_qMenu->Add(new MenuBool("harassQ", "Use Q on minion", true));

        m_wMenu->Add(new MenuBool("autoW",     "Auto W", true));
        m_wMenu->Add(new MenuBool("ignoreCol", "Ignore collision", true));
        m_wMenu->Add(new MenuBool("wInAaRange","Cast only in AA range", true));

        m_eMenu->Add(new MenuBool("autoE", "Auto E", true));
        m_eMenu->Add(new MenuBool("slowE", "Auto SlowBuff E", true));
        // TODO(oktw-port): Core.OKTWdash instance (Lucian dash prediction helper)

        m_rMenu->Add(new MenuBool("autoR", "Auto R", true));
        m_rMenu->Add(new MenuKeyBind("useR", "Semi-manual cast R key", 'T', SDK::KeyBindType::Press));

        m_farmMenu->Add(new MenuBool("farmQ", "LaneClear Q", true));
        m_farmMenu->Add(new MenuBool("farmW", "LaneClear W", true));

        // TODO(oktw-port): Obj_AI_Base.OnProcessSpellCast — sets m_passRdy for LucianQ/W/E and m_castR on LucianR
        // TODO(oktw-port): Spellbook.OnCastSpell — sets m_passRdy on Q/W/E slots
        // TODO(oktw-port): SebbyLib.Orbwalking.AfterAttack (no-op body, kept for parity)
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

    bool SpellLock() const { return Player().HasBuff("lucianpassivebuff"); }

    void OnGameUpdate() override {
        const auto p = Player();
        if (!p.IsValid()) return;

        // C# chaneling — issue-move override during important channels
        if (false && // TODO(oktw-port): IsChannelingImportantSpell not available in SDK
            (static_cast<int>(SDK::Game::Time() * 10.0f) % 2 == 0)) {
            // TODO(oktw-port): Player.IssueOrder(MoveTo, Game.CursorPos)
        }

        if (m_R1.IsReady() && SDK::Game::Time() - m_castR > 5.0f && GetKey("useR")) {
            auto* ts = SDK::TargetSelector::Instance();
            auto t = ts ? ts->GetTarget(m_R.Range, DamageType::Physical) : AIHeroClient();
            if (t.IsValid() && SDK::Extensions::IsValidTarget(t, m_R1.Range, true)) {
                m_R1.Cast(t);
                return;
            }
        }

        if (LagFree(0)) SetMana();
        if (LagFree(1) && m_Q.IsReady() && !m_passRdy && !SpellLock())
            LogicQ();
        if (LagFree(2) && m_W.IsReady() && !m_passRdy && !SpellLock() && GetBool("autoW"))
            LogicW();
        if (LagFree(3) && m_E.IsReady())
            LogicE();
        if (LagFree(4)) {
            if (m_R.IsReady() && SDK::Game::Time() - m_castR > 5.0f && GetBool("autoR"))
                LogicR();

            if (!m_passRdy && !SpellLock())
                Farm();
        }
    }

    float AaDamage(const AIHeroClient& target) {
        const auto p = Player();
        const float aa = p.GetAutoAttackDamage(target, false);
        const int lvl = static_cast<int>(p.Level());
        if (lvl > 12) return aa * 1.3f;
        if (lvl > 6)  return aa * 1.4f;
        if (lvl > 0)  return aa * 1.5f;
        return 0.0f;
    }

    void LogicQ() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t  = ts ? ts->GetTarget(m_Q.Range,  DamageType::Physical) : AIHeroClient();
        auto t1 = ts ? ts->GetTarget(m_Q1.Range, DamageType::Physical) : AIHeroClient();
        const auto p = Player();

        if (t.IsValid() && SDK::Extensions::IsValidTarget(t, m_Q.Range, true)) {
            if (OktwCommon::GetKsDamage(t, m_Q) + AaDamage(t) > t.Health()) {
                m_Q.Cast(t);
            } else if (Combo() && p.Mana() > m_RMANA + m_QMANA) {
                m_Q.Cast(t);
            } else if (Harass() &&
                       GetBool((std::string("Harass") + t.CharacterName()).c_str()) &&
                       p.Mana() > m_RMANA + m_QMANA + m_EMANA + m_WMANA) {
                m_Q.Cast(t);
            }
        } else if ((Harass() || Combo()) && GetBool("harassQ") &&
                   t1.IsValid() && SDK::Extensions::IsValidTarget(t1, m_Q1.Range, true) &&
                   GetBool((std::string("Harass") + t1.CharacterName()).c_str()) &&
                   p.Position().Distance(t1.ServerPosition()) > m_Q.Range + 100.0f) {
            if (Combo() && p.Mana() < m_RMANA + m_QMANA) return;
            if (Harass() && p.Mana() < m_RMANA + m_QMANA + m_EMANA + m_WMANA) return;
            if (!OktwCommon::CanHarras()) return;

            const auto prepos = m_Q1.GetPrediction(t1);
            if (static_cast<int>(prepos.Hitchance) < static_cast<int>(HitChance::High)) return;

            const Vector3 castPos = prepos.GetCastPosition();
            const float distance = p.Position().Distance(castPos);
            auto minions = OktwCommon::GetMinions(p.ServerPosition(), m_Q.Range);
            for (const auto& minion : minions) {
                if (!minion.IsValid()) continue;
                if (!SDK::Extensions::IsValidTarget(minion, m_Q.Range, true)) continue;
                // Extend(Player.Position, minion.Position, distance)
                const Vector3 diff = minion.Position() - p.Position();
                const float len = diff.Length();
                if (len < 0.001f) continue;
                const Vector3 extended = p.Position() + diff * (distance / len);
                if (castPos.Distance(extended) < 25.0f) {
                    m_Q.Cast(minion);
                    return;
                }
            }
        }
    }

    void LogicW() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_W.Range, DamageType::Physical) : AIHeroClient();
        if (!t.IsValid()) return;

        // Ignore-collision toggle when target is in AA range
        if (GetBool("ignoreCol") && SDK::Core::Utils::AutoAttack::InAutoAttackRange(t))
            m_W.Collision = false;
        else
            m_W.Collision = true;

        const auto p = Player();
        float qDmg = m_Q.GetDamage(t);
        float wDmg = OktwCommon::GetKsDamage(t, m_W);

        if (SDK::Core::Utils::AutoAttack::InAutoAttackRange(t)) {
            qDmg += AaDamage(t);
            wDmg += AaDamage(t);
        }

        if (wDmg > t.Health()) {
            CastSpell(m_W, t);
        } else if (wDmg + qDmg > t.Health() && m_Q.IsReady() &&
                   p.Mana() > m_RMANA + m_WMANA + m_QMANA) {
            CastSpell(m_W, t);
        }

        // orbT resolution — Orbwalker::GetTarget is unavailable; fall back to
        // "cast only in AA range" gate (matches C# when orbT is null).
        // TODO(oktw-port): Orbwalker::GetTarget() (AA target hero) — using TS fallback
        if (GetBool("wInAaRange") && !SDK::Core::Utils::AutoAttack::InAutoAttackRange(t)) {
            return;
        }

        if (SDK::Orbwalker::ActiveMode() == SDK::OrbwalkingMode::Combo &&
            p.Mana() > m_RMANA + m_WMANA + m_EMANA + m_QMANA) {
            CastSpell(m_W, t);
        } else if (Harass() &&
                   GetBool((std::string("Harass") + t.CharacterName()).c_str()) &&
                   !p.IsUnderEnemyTurret() &&
                   p.Mana() > p.MaxMana() * 0.8f &&
                   p.Mana() > m_RMANA + m_WMANA + m_EMANA + m_QMANA + m_WMANA) {
            CastSpell(m_W, t);
        } else if ((Combo() || Harass()) && p.Mana() > m_RMANA + m_WMANA + m_EMANA) {
            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                if (SDK::Extensions::IsValidTarget(enemy, m_W.Range, true) &&
                    !OktwCommon::CanMove(enemy)) {
                    m_W.Cast(enemy);
                }
            }
        }
    }

    void LogicR() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_R.Range, DamageType::Physical) : AIHeroClient();
        if (!t.IsValid()) return;
        if (!SDK::Extensions::IsValidTarget(t, m_R.Range, true)) return;
        if (OktwCommon::CountAlliesInRange(t.Position(), 500.0f) != 0) return;
        if (!OktwCommon::ValidUlt(t)) return;
        if (SDK::Core::Utils::AutoAttack::InAutoAttackRange(t)) return;

        const auto p = Player();
        const int rLvl = static_cast<int>(p.GetSpell(SpellSlot::R).Level());
        const float rDmg = m_R.GetDamage(t, SDK::DamageStage::SecondForm) *
                           static_cast<float>(10 + 5 * rLvl);
        const float tDis = p.Position().Distance(t.ServerPosition());

        if (rDmg * 0.8f > t.Health() && tDis < 700.0f && !m_Q.IsReady())      m_R.Cast(t);
        else if (rDmg * 0.7f > t.Health() && tDis < 800.0f)                    m_R.Cast(t);
        else if (rDmg * 0.6f > t.Health() && tDis < 900.0f)                    m_R.Cast(t);
        else if (rDmg * 0.5f > t.Health() && tDis < 1000.0f)                   m_R.Cast(t);
        else if (rDmg * 0.4f > t.Health() && tDis < 1100.0f)                   m_R.Cast(t);
        else if (rDmg * 0.3f > t.Health() && tDis < 1200.0f)                   m_R.Cast(t);
    }

    void LogicE() {
        const auto p = Player();
        if (p.Mana() < m_RMANA + m_EMANA || !GetBool("autoE")) return;

        // Melee-in-range panic dash — TODO: OKTWdash absent, best-effort dash away from
        // the nearest melee threat along the (self - threat) vector.
        // TODO(oktw-port): Core.OKTWdash.CastDash — using directional fallback
        bool meleeInRange = false;
        Vector3 threatPos{};
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            if (SDK::Extensions::IsValidTarget(enemy, 270.0f, true) && enemy.IsMelee()) {
                meleeInRange = true;
                threatPos = enemy.ServerPosition();
                break;
            }
        }

        if (meleeInRange) {
            const Vector3 diff = p.Position() - threatPos;
            const float len = diff.Length();
            if (len > 0.001f) {
                const Vector3 dashPos = p.Position() + diff * (m_E.Range / len);
                m_E.Cast(dashPos);
            }
            return;
        }

        if (!Combo() || m_passRdy || SpellLock()) return;

        // Combo dash toward cursor at E.Range
        const Vector3 cursor = SDK::Game::CursorPos();
        const Vector3 diff = cursor - p.Position();
        const float len = diff.Length();
        if (len > 0.001f) {
            const Vector3 dashPos = p.Position() + diff * (m_E.Range / len);
            m_E.Cast(dashPos);
        }
    }

    void Farm() {
        const auto p = Player();
        if (!LaneClear() || p.Mana() <= m_RMANA + m_QMANA) return;

        auto mobs = OktwCommon::GetMinions(p.ServerPosition(), m_Q.Range, false, true);
        if (!mobs.empty()) {
            const auto& mob = mobs.front();
            if (m_Q.IsReady()) { m_Q.Cast(mob); return; }
            if (m_W.IsReady()) { m_W.Cast(mob.Position()); return; }
        }

        if (!FarmSpells()) return;

        if (m_Q.IsReady() && GetBool("farmQ")) {
            auto minions = OktwCommon::GetMinions(p.ServerPosition(), m_Q1.Range);
            for (const auto& minion : minions) {
                auto poutput = m_Q1.GetPrediction(minion);
                if (static_cast<int>(poutput.CollisionObjects.size()) > 2) {
                    const auto& minionQ = poutput.CollisionObjects.front();
                    if (minionQ.IsValid() &&
                        SDK::Extensions::IsValidTarget(minionQ, m_Q.Range, true)) {
                        m_Q.Cast(minion);
                        return;
                    }
                }
            }
        }
        if (m_W.IsReady() && GetBool("farmW")) {
            auto minions = OktwCommon::GetMinions(p.ServerPosition(), m_Q1.Range);
            std::vector<SDK::AIBaseClient> baseList(minions.begin(), minions.end());
            auto Wfarm = m_W.GetCircularFarmLocation(baseList, 150.0f);
            if (Wfarm.MinionsHit > 3) m_W.Cast(Wfarm.Position);
        }
    }

    void OnGameDraw() override {}
};

} } // namespace Plugins::OKTW
