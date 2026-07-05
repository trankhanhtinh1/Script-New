#pragma once
// Port of OKTW_CSharp/Champions/Corki.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;
using SDK::UI::MenuKeyBind;

class CorkiPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Corki"; }
    const char* GetInternalId() const override { return "champion.oktw.corki"; }
    const char* GetChampionName() const override { return "Corki"; }

protected:
    void BuildMenu() override {
        MarkActive();

        m_Q = Spell(SpellSlot::Q, 825.0f);
        m_W = Spell(SpellSlot::W, 600.0f);
        m_E = Spell(SpellSlot::E, 800.0f);
        m_R = Spell(SpellSlot::R, 1230.0f);

        m_Q.SetSkillshot(0.3f, 200.0f, 1000.0f, false, SDK::SpellType::Circle);
        m_R.SetSkillshot(0.2f,  40.0f, 2000.0f, true,  SDK::SpellType::Line);

        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw only ready spells", true));
        m_drawMenu->Add(new MenuBool("qRange",  "Q range", false));
        m_drawMenu->Add(new MenuBool("wRange",  "W range", false));
        m_drawMenu->Add(new MenuBool("eRange",  "E range", false));
        m_drawMenu->Add(new MenuBool("rRange",  "R range", false));

        m_qMenu->Add(new MenuBool("autoQ",   "Auto Q",  true));
        m_qMenu->Add(new MenuBool("harassQ", "Q harass", true));

        m_wMenu->Add(new MenuBool("nktdE", "NoKeyToDash", true));

        m_eMenu->Add(new MenuBool("autoE",   "Auto E",   true));
        m_eMenu->Add(new MenuBool("harassE", "E harass", true));

        m_rMenu->Add(new MenuBool("autoR",  "Auto R", true));
        m_rMenu->Add(new MenuSlider("Rammo",   "Minimum R ammo harass", 3, 0, 6));
        m_rMenu->Add(new MenuBool("minionR", "Try R on minion", true));
        m_rMenu->Add(new MenuKeyBind("useR",  "Semi-manual cast R key", 'T', SDK::KeyBindType::Press));

        m_farmMenu->Add(new MenuSlider("RammoLC", "Minimum R ammo Lane clear", 3, 0, 6));
        m_farmMenu->Add(new MenuBool("farmQ", "LaneClear + jungle Q", true));
        m_farmMenu->Add(new MenuBool("farmR", "LaneClear + jungle R", true));
    }

    // ── Buffs ─────────────────────────────────────────────────────────────
    bool bonusR() const { return Player().HasBuff("corkimissilebarragecounterbig"); }

    // Sheen: return true when it's OK to spell-weave (i.e., NOT holding a
    // Sheen-empowered auto for the current target). Mirrors C# Sheen().
    bool Sheen() const {
        // TODO(SDK): SDK::Orbwalker::GetTarget for current attack target.
        // Fallback approximation: only block spells when Sheen buff is up.
        return !Player().HasBuff("sheen");
    }

    void SetMana() override {
        const auto p = Player();
        if ((Shared().manaDisable && Shared().manaDisable->Value && Combo()) ||
            p.HealthPercent() < 20.0f) {
            m_QMANA = m_WMANA = m_EMANA = m_RMANA = 0.0f;
            return;
        }
        m_QMANA = m_Q.Instance().ManaCost();
        m_WMANA = m_W.Instance().ManaCost();
        m_EMANA = m_E.Instance().ManaCost();

        if (!m_R.IsReady()) {
            // C#: RMANA = QMANA - PARRegenRate * Q.Cooldown
            // TODO(SDK): PARRegenRate not exposed — approximate with 0 to preserve QMANA gating.
            m_RMANA = m_QMANA;
        } else {
            m_RMANA = m_R.Instance().ManaCost();
        }
    }

    // ── Main tick ────────────────────────────────────────────────────────
    void OnGameUpdate() override {
        const auto p = Player();
        if (!p.IsValid()) return;

        // TODO(SDK): SebbyLib Orbwalking.BeforeAttack hook is not available.
        // Approximation: run the Sheen-E logic each tick against the current
        // Orbwalker target when E is ready and Sheen check passes.
        RunBeforeAttack();

        if (LagFree(0)) {
            SetMana();
            Farm();
        }
        // Note: C# gates LogicQ/LogicR on !Player.Spellbook.IsAutoAttacking.
        // TODO(SDK): expose IsAutoAttacking — skipping that gate here.
        if (LagFree(1) && m_Q.IsReady() && Sheen()) LogicQ();
        if (LagFree(2) && Combo() && m_W.IsReady()) LogicW();
        if (LagFree(4) && m_R.IsReady() && Sheen()) LogicR();
    }

    // ── BeforeAttack: Sheen-empowered E weave ────────────────────────────
    void RunBeforeAttack() {
        if (!m_E.IsReady() || Sheen()) return;
        // Sheen() == false means we are currently Sheen-buffed and about to hit.
        // TODO(SDK): SDK::Orbwalker::GetTarget — using TS fallback.
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_E.Range, SDK::DamageType::Physical)
                    : SDK::AIHeroClient();
        if (!t.IsValid()) return;

        const auto p = Player();
        if (Combo() && GetBool("autoE") && p.Mana() > m_EMANA + m_RMANA) {
            m_E.Cast(t.Position());
        }
        if (Harass() && GetBool("harassE") &&
            p.Mana() > m_EMANA + m_RMANA + m_QMANA && OktwCommon::CanHarras()) {
            m_E.Cast(t.Position());
        }
        if (!m_Q.IsReady() && !m_R.IsReady() &&
            t.Health() < p.BonusAttackDamage() * 2.0f) {
            m_E.Cast();
        }
    }

    // ── R ────────────────────────────────────────────────────────────────
    void LogicR() {
        const auto p = Player();
        float rSplash = 150.0f;
        if (bonusR()) rSplash = 300.0f;

        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_R.Range, SDK::DamageType::Physical)
                    : SDK::AIHeroClient();

        if (!t.IsValid()) return;

        const float rDmg = OktwCommon::GetKsDamage(t, m_R);
        const float qDmg = m_Q.GetDamage(t);

        if (rDmg * 2.0f > t.Health()) {
            CastR(t);
        } else if (SDK::Extensions::IsValidTarget(t, m_Q.Range, true) &&
                   qDmg + rDmg > t.Health()) {
            CastR(t);
        }

        // TODO(SDK): Spellbook.GetSpell(R).Ammo — approximate with an SDK helper if
        // exposed via Spell::Ammo(); otherwise treat as >1 so multi-cast logic runs.
        int rAmmo = 2;
        // rAmmo = m_R.Ammo();  // uncomment once SDK exposes Ammo()

        if (rAmmo > 1) {
            SDK::AIHeroClient best = t;
            for (const auto& enemy : SDK::ObjectManager::Get<SDK::AIHeroClient>()) {
                if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                if (!SDK::Extensions::IsValidTarget(enemy, m_R.Range, true)) continue;
                if (OktwCommon::CountEnemiesInRange(enemy.Position(), rSplash) > 1) {
                    best = enemy;
                }
            }

            if (Combo() && p.Mana() > m_RMANA * 3.0f) {
                CastR(best);
            } else if (Harass() &&
                       p.Mana() > m_RMANA + m_EMANA + m_QMANA + m_WMANA &&
                       rAmmo >= GetSlider("Rammo", 3) &&
                       OktwCommon::CanHarras()) {
                for (const auto& enemy : SDK::ObjectManager::Get<SDK::AIHeroClient>()) {
                    if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                    if (!SDK::Extensions::IsValidTarget(enemy, m_R.Range, true)) continue;
                    if (!GetBool((std::string("Harass") + enemy.CharacterName()).c_str())) continue;
                    CastR(enemy);
                }
            }

            if (!None() && p.Mana() > m_RMANA + m_QMANA + m_EMANA) {
                for (const auto& enemy : SDK::ObjectManager::Get<SDK::AIHeroClient>()) {
                    if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                    if (!SDK::Extensions::IsValidTarget(enemy, m_R.Range, true)) continue;
                    if (!OktwCommon::CanMove(enemy)) CastR(best);
                }
            }
        }
    }

    void CastR(const SDK::AIHeroClient& t) {
        CastSpell(m_R, t);

        if (!GetBool("minionR", true)) return;

        const auto poutput = m_R.GetPrediction(t);
        int col = 0;
        for (const auto& obj : poutput.CollisionObjects) {
            if (obj.IsEnemy() && obj.IsMinion() && !obj.IsDead()) ++col;
        }

        const auto prepos = SDK::Prediction::GetPrediction(t, 0.4f);
        // HitChance values: VeryHigh=6, High=5, Medium=4, Low=3, Impossible=2, Collision=1, OutOfRange=0
        // C# uses "< 5" → below High → skip.
        if (col == 0 && static_cast<int>(prepos.Hitchance) < static_cast<int>(SDK::HitChance::High)) {
            return;
        }

        float rSplash = 140.0f;
        if (bonusR()) rSplash = 290.0f;

        const auto minions = OktwCommon::GetMinions(Player().ServerPosition(), m_R.Range - rSplash);
        const Vector3 cast = poutput.GetCastPosition();
        for (const auto& minion : minions) {
            const Vector3 mp = minion.Position();
            const float dx = mp.x - cast.x;
            const float dz = mp.z - cast.z;
            if (dx * dx + dz * dz < rSplash * rSplash) {
                m_R.Cast(minion);
                return;
            }
        }
    }

    // ── W ────────────────────────────────────────────────────────────────
    void LogicW() {
        const auto p = Player();
        const Vector3 cursor = SDK::Game::CursorPos();
        const Vector3 pos = p.Position();

        // Player.Position.Extend(cursor, W.Range)
        const Vector3 diff = { cursor.x - pos.x, cursor.y - pos.y, cursor.z - pos.z };
        const float len = std::sqrt(diff.x * diff.x + diff.z * diff.z);
        Vector3 dashPos = pos;
        if (len > 0.001f) {
            const float k = m_W.Range / len;
            dashPos = { pos.x + diff.x * k, pos.y, pos.z + diff.z * k };
        }

        const float cursorDist = pos.Distance(cursor);
        if (cursorDist > p.AttackRange() + p.BoundingRadius() * 2.0f &&
            Combo() && GetBool("nktdE", true) &&
            p.Mana() > m_RMANA + m_WMANA - 10.0f) {
            m_W.Cast(dashPos);
        }
    }

    // ── Q ────────────────────────────────────────────────────────────────
    void LogicQ() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_Q.Range, SDK::DamageType::Physical)
                    : SDK::AIHeroClient();

        if (t.IsValid()) {
            const auto p = Player();
            if (Combo() && GetBool("autoQ", true) && p.Mana() > m_RMANA + m_QMANA) {
                CastSpell(m_Q, t);
            } else if (Harass() && GetBool("harassQ", true) &&
                       GetBool((std::string("Harass") + t.CharacterName()).c_str()) &&
                       p.Mana() > m_RMANA + m_EMANA + m_WMANA + m_RMANA &&
                       OktwCommon::CanHarras()) {
                CastSpell(m_Q, t);
            } else {
                const float qDmg = OktwCommon::GetKsDamage(t, m_Q);
                const float rDmg = m_R.GetDamage(t);
                if (qDmg > t.Health()) {
                    m_Q.Cast(t);
                } else if (rDmg + qDmg > t.Health() && p.Mana() > m_RMANA + m_QMANA) {
                    CastSpell(m_Q, t);
                } else if (rDmg + 2.0f * qDmg > t.Health() &&
                           p.Mana() > m_QMANA + m_RMANA * 2.0f) {
                    CastSpell(m_Q, t);
                }
            }

            if (!None() && p.Mana() > m_RMANA + m_WMANA + m_EMANA) {
                for (const auto& enemy : SDK::ObjectManager::Get<SDK::AIHeroClient>()) {
                    if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                    if (!SDK::Extensions::IsValidTarget(enemy, m_Q.Range, true)) continue;
                    if (!OktwCommon::CanMove(enemy)) m_Q.Cast(enemy);
                }
            }
        }
    }

    // ── Farm ─────────────────────────────────────────────────────────────
    void Farm() {
        // TODO(SDK): C# also gates on !Player.Spellbook.IsAutoAttacking.
        if (!LaneClear() || !Sheen()) return;

        const auto p = Player();
        auto mobs = OktwCommon::GetMinions(p.ServerPosition(), m_Q.Range, false, /*jungleOnly=*/true);
        if (!mobs.empty() && p.Mana() > m_RMANA + m_WMANA + m_EMANA + m_QMANA) {
            const auto& mob = mobs.front();
            if (m_Q.IsReady() && GetBool("farmQ", true)) {
                m_Q.Cast(mob.Position());
                return;
            }
            if (m_R.IsReady() && GetBool("farmR", true)) {
                m_R.Cast(mob.Position());
                return;
            }
        }

        if (!FarmSpells()) return;

        auto minions = OktwCommon::GetMinions(p.ServerPosition(), m_Q.Range);
        if (minions.empty()) return;

        // TODO(SDK): Spellbook.GetSpell(R).Ammo — approximate with slider satisfied.
        int rAmmo = 6;
        // rAmmo = m_R.Ammo();

        if (m_R.IsReady() && GetBool("farmR", true) && rAmmo >= GetSlider("RammoLC", 3)) {
            std::vector<SDK::AIBaseClient> base(minions.begin(), minions.end());
            const auto rfarm = m_R.GetCircularFarmLocation(base, 100.0f);
            if (rfarm.MinionsHit >= FarmMinions()) {
                m_R.Cast(rfarm.Position);
                return;
            }
        }
        if (m_Q.IsReady() && GetBool("farmQ", true)) {
            std::vector<SDK::AIBaseClient> base(minions.begin(), minions.end());
            const auto qfarm = m_Q.GetCircularFarmLocation(base, m_Q.Width);
            if (qfarm.MinionsHit >= FarmMinions()) {
                m_Q.Cast(qfarm.Position);
                return;
            }
        }
    }

    // ── Draw ─────────────────────────────────────────────────────────────
    void OnGameDraw() override {
        // Range circles — SDK utilities used elsewhere in the AIO handle these.
        // TODO(SDK): DrawCircle/WorldToScreen text — port kept parity in menu only.
    }
};

} } // namespace Plugins::OKTW
