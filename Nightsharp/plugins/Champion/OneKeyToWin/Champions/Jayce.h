#pragma once
// Port of OKTW_CSharp/Champions/Jayce.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;
using SDK::UI::MenuKeyBind;

class JaycePlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Jayce"; }
    const char* GetInternalId() const override { return "champion.oktw.jayce"; }
    const char* GetChampionName() const override { return "Jayce"; }

protected:
    // Ranged (cannon) form spells
    Spell m_Qext{ SpellSlot::Q };
    Spell m_QextCol{ SpellSlot::Q };
    // Melee (hammer) form spells
    Spell m_Q1{ SpellSlot::Q };
    Spell m_W1{ SpellSlot::W };
    Spell m_E1{ SpellSlot::E };

    float m_QMANA2 = 0.0f, m_WMANA2 = 0.0f, m_EMANA2 = 0.0f;
    float m_Qcd = 0.0f, m_Wcd = 0.0f, m_Ecd = 0.0f;
    float m_Q2cd = 0.0f, m_W2cd = 0.0f, m_E2cd = 0.0f;

    static constexpr int kMuramana = 3042;
    static constexpr int kTear     = 3070;
    static constexpr int kManamune = 3004;

    bool IsRange() const {
        // In C#: Q.Instance.Name.ToLower() == "jayceshockblast"
        // Approximate via a buff / spell name check on the Q slot when available.
        // TODO(oktw-port): expose actual spell name from Instance when SDK supports it.
        return Player().HasBuff("JayceStanceHtG") == false;
    }

    void BuildMenu() override {
        MarkActive();

        // Ranged form spells
        m_Q     = Spell(SpellSlot::Q, 1030.0f);
        m_Qext  = Spell(SpellSlot::Q, 1650.0f);
        m_QextCol = Spell(SpellSlot::Q, 1650.0f);
        m_W     = Spell(SpellSlot::W);
        m_E     = Spell(SpellSlot::E, 650.0f);
        m_R     = Spell(SpellSlot::R);

        // Melee form spells
        m_Q1 = Spell(SpellSlot::Q, 600.0f);
        m_W1 = Spell(SpellSlot::W, 350.0f);
        m_E1 = Spell(SpellSlot::E, 240.0f);

        m_Q.SetSkillshot(0.25f, 70.0f, 1450.0f, true, SDK::SpellType::Line);
        m_Qext.SetSkillshot(0.30f, 80.0f, 2000.0f, false, SDK::SpellType::Line);
        m_QextCol.SetSkillshot(0.30f, 100.0f, 1600.0f, true, SDK::SpellType::Line);
        m_Q1.SetTargetted(0.25f, FLT_MAX);
        m_E.SetSkillshot(0.10f, 120.0f, FLT_MAX, false, SDK::SpellType::Circle);
        m_E1.SetTargetted(0.25f, FLT_MAX);

        // Draw
        m_drawMenu->Add(new MenuBool("showcd",  "Show cooldown", true));
        m_drawMenu->Add(new MenuBool("noti",    "Show notification & line", true));
        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw only ready spells", true));
        m_drawMenu->Add(new MenuBool("qRange",  "Q range", false));

        // Q Config
        m_qMenu->Add(new MenuBool("autoQ",  "Auto Q range", true));
        m_qMenu->Add(new MenuBool("autoQm", "Auto Q melee", true));
        m_qMenu->Add(new MenuBool("QEforce", "force E + Q", false));
        m_qMenu->Add(new MenuBool("QEsplash", "Q + E splash minion damage", true));
        m_qMenu->Add(new MenuSlider("QEsplashAdjust", "Q + E splash minion radius", 150, 50, 250));
        m_qMenu->Add(new MenuKeyBind("useQE", "Semi-manual Q + E near mouse key", 'T', SDK::KeyBindType::Press));

        // W Config
        m_wMenu->Add(new MenuBool("autoW",     "Auto W range", true));
        m_wMenu->Add(new MenuBool("autoWm",    "Auto W melee", true));
        m_wMenu->Add(new MenuBool("autoWmove", "Disable move if W range active", true));

        // E Config
        m_eMenu->Add(new MenuBool("autoE",   "Auto E range (Q + E)", true));
        m_eMenu->Add(new MenuBool("autoEm",  "Auto E melee", true));
        m_eMenu->Add(new MenuBool("autoEks", "E melee ks only", false));
        m_eMenu->Add(new MenuBool("gapE",    "Gapcloser R + E", true));
        m_eMenu->Add(new MenuBool("intE",    "Interrupt spells R + Q + E", true));

        // R Config
        m_rMenu->Add(new MenuBool("autoR",  "Auto R range", true));
        m_rMenu->Add(new MenuBool("autoRm", "Auto R melee", true));

        // Root-level items
        m_drawMenu->Add(new MenuBool("stack", "Stack Tear if full mana", false));

        // Harass mana slider
        m_harassMenu->Add(new MenuSlider("harassMana", "Harass Mana", 80, 0, 100));

        // Flee key
        m_drawMenu->Add(new MenuKeyBind("flee", "FLEE MODE", 'T', SDK::KeyBindType::Press));

        // Farm
        m_farmMenu->Add(new MenuBool("farmQ",    "Lane clear Q + E range", true));
        m_farmMenu->Add(new MenuBool("farmW",    "Lane clear W range && mele", true));
        m_farmMenu->Add(new MenuBool("jungleQ",  "Jungle clear Q", true));
        m_farmMenu->Add(new MenuBool("jungleW",  "Jungle clear W", true));
        m_farmMenu->Add(new MenuBool("jungleE",  "Jungle clear E", true));
        m_farmMenu->Add(new MenuBool("jungleR",  "Jungle clear R", true));
        m_farmMenu->Add(new MenuBool("jungleQm", "Jungle clear Q melee", true));
        m_farmMenu->Add(new MenuBool("jungleWm", "Jungle clear W melee", true));
        m_farmMenu->Add(new MenuBool("jungleEm", "Jungle clear E melee", true));
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

    // TODO(oktw-port): wire when SDK event exposed
    void AntiGapcloser_OnEnemyGapcloser() {
        // TODO(oktw-port): wire when SDK event exposed
    }

    // TODO(oktw-port): wire when SDK event exposed
    void Interrupter_OnInterruptable() {
        // TODO(oktw-port): wire when SDK event exposed
    }

    // TODO(oktw-port): wire when SDK event exposed
    void Spellbook_OnCastSpell() {
        // TODO(oktw-port): wire when SDK event exposed
    }

    // TODO(oktw-port): wire when SDK event exposed
    void OnProcessSpellCast_Self() {
        // TODO(oktw-port): wire when SDK event exposed
    }

    // TODO(oktw-port): wire when SDK event exposed
    void BeforeAttack() {
        // TODO(oktw-port): wire when SDK event exposed
    }

    void OnGameUpdate() override {
        const auto p = Player();
        if (!p.IsValid() || p.HasBuff("Recall")) return;

        if (GetKey("flee")) {
            FleeMode();
        }

        if (LagFree(0)) { SetMana(); }

        if (IsRange()) {
            if (LagFree(1) && m_Q.IsReady() && GetBool("autoQ")) LogicQ();
            if (LagFree(2) && m_W.IsReady() && GetBool("autoW")) LogicW();
        } else {
            if (LagFree(1) && m_E1.IsReady() && GetBool("autoEm")) LogicE2();
            if (LagFree(2) && m_Q1.IsReady() && GetBool("autoQm")) LogicQ2();
            if (LagFree(3) && m_W1.IsReady() && GetBool("autoWm")) LogicW2();
        }

        if (LagFree(4)) {
            SetValue();
            if (m_R.IsReady()) LogicR();
        }

        Jungle();
        LaneClearLogic();
    }

    void FleeMode() {
        const auto p = Player();
        if (IsRange()) {
            if (m_E.IsReady()) {
                const Vector3 cursor = SDK::Game::CursorPos();
                const Vector3 dir = (cursor - p.Position());
                const float len = dir.Length();
                Vector3 target = (len > 0.001f) ? (p.Position() + dir * (150.0f / len)) : p.Position();
                m_E.Cast(target);
            } else if (m_R.IsReady()) {
                m_R.Cast();
            }
        } else {
            if (m_Q1.IsReady()) {
                auto mobs = OktwCommon::GetMinions(p.ServerPosition(), m_Q1.Range);
                if (!mobs.empty()) {
                    const Vector3 cursor = SDK::Game::CursorPos();
                    const auto* best = &mobs.front();
                    for (const auto& mob : mobs) {
                        if (!SDK::Extensions::IsValidTarget(mob, m_Q1.Range, true)) continue;
                        if (mob.Position().Distance(cursor) < best->Position().Distance(cursor)) {
                            best = &mob;
                        }
                    }
                    if (best->Position().Distance(cursor) + 200.0f < p.Position().Distance(cursor)) {
                        m_Q1.Cast(*best);
                    }
                } else if (m_R.IsReady()) {
                    m_R.Cast();
                }
            } else if (m_R.IsReady()) {
                m_R.Cast();
            }
        }
    }

    bool CanUseQE() const {
        return m_E.IsReady() && Player().Mana() > m_QMANA + m_EMANA && GetBool("autoE");
    }

    void CastQ(const AIHeroClient& t) {
        if (!CanUseQE()) {
            CastSpell(m_Q, t);
            return;
        }
        bool cast = true;
        if (GetBool("QEsplash")) {
            const auto pout = m_QextCol.GetPrediction(t, true);
            const float adjust = static_cast<float>(GetSlider("QEsplashAdjust", 150));
            for (const auto& minion : pout.CollisionObjects) {
                if (minion.IsEnemy() && minion.Position().Distance(pout.GetCastPosition()) > adjust) {
                    cast = false;
                    break;
                }
            }
        } else {
            cast = false;
        }
        if (cast) CastSpell(m_Qext, t);
        else      CastSpell(m_QextCol, t);
    }

    void LogicQ() {
        Spell qType = m_Q;
        if (CanUseQE()) {
            qType = m_Qext;
            if (GetKey("useQE")) {
                const Vector3 cursor = SDK::Game::CursorPos();
                const AIHeroClient* mouseTarget = nullptr;
                float bestDist = FLT_MAX;
                for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                    if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                    if (!SDK::Extensions::IsValidTarget(enemy, qType.Range, true)) continue;
                    const float d = enemy.Position().Distance(cursor);
                    if (d < bestDist) { bestDist = d; mouseTarget = &enemy; }
                }
                if (mouseTarget) { CastQ(*mouseTarget); return; }
            }
        }

        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(qType.Range, DamageType::Physical) : AIHeroClient();
        if (!t.IsValid()) return;

        const auto p = Player();
        float qDmg = OktwCommon::GetKsDamage(t, qType);
        if (CanUseQE()) qDmg *= 1.4f;

        if (qDmg > t.Health()) {
            CastQ(t);
        } else if (Combo() && p.Mana() > m_EMANA + m_QMANA) {
            CastQ(t);
        } else if (Harass() &&
                   p.ManaPercent() > static_cast<float>(GetSlider("harassMana", 80)) &&
                   OktwCommon::CanHarras()) {
            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                if (!SDK::Extensions::IsValidTarget(enemy, qType.Range, true)) continue;
                if (!GetBool((std::string("Harass") + enemy.CharacterName()).c_str())) continue;
                CastQ(t);
            }
        } else if ((Combo() || Harass()) && p.Mana() > m_RMANA + m_QMANA + m_EMANA) {
            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                if (!SDK::Extensions::IsValidTarget(enemy, qType.Range, true)) continue;
                if (!OktwCommon::CanMove(enemy)) CastQ(t);
            }
        }
    }

    void LogicW() {
        if (!Combo() || !m_R.IsReady() || !IsRange()) return;
        // Orbwalker.GetTarget() is a hero and valid: approximate via nearest enemy hero in AA range
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            if (SDK::Extensions::IsValidTarget(enemy, 600.0f, true)) {
                m_W.Cast();
                return;
            }
        }
    }

    void LogicQ2() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_Q1.Range, DamageType::Physical) : AIHeroClient();
        if (!t.IsValid()) return;
        if (OktwCommon::GetKsDamage(t, m_Q1) > t.Health()) {
            m_Q1.Cast(t);
        } else if (Combo() && Player().Mana() > m_RMANA + m_QMANA) {
            m_Q1.Cast(t);
        }
    }

    void LogicW2() {
        if (OktwCommon::CountEnemiesInRange(Player().Position(), 300.0f) > 0 &&
            Player().Mana() > 80.0f) {
            m_W.Cast();
        }
    }

    void LogicE2() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_E1.Range, DamageType::Physical) : AIHeroClient();
        if (!t.IsValid()) return;
        if (OktwCommon::GetKsDamage(t, m_E1) > t.Health()) {
            m_E1.Cast(t);
        } else if (Combo() && !GetBool("autoEks") && !Player().HasBuff("jaycehyperchargevfx")) {
            m_E1.Cast(t);
        }
    }

    void LogicR() {
        const auto p = Player();
        if (IsRange() && GetBool("autoRm")) {
            auto* ts = SDK::TargetSelector::Instance();
            auto t = ts ? ts->GetTarget(m_Q1.Range + 200.0f, DamageType::Physical) : AIHeroClient();
            if (Combo() && m_Qcd > 0.5f && t.IsValid() &&
                ((!m_W.IsReady() && !t.IsMelee()) ||
                 (!m_W.IsReady() && !p.HasBuff("jaycehyperchargevfx") && t.IsMelee()))) {
                if (m_Q2cd < 0.5f && OktwCommon::CountEnemiesInRange(t.Position(), 800.0f) < 3) {
                    m_R.Cast();
                } else if (OktwCommon::CountEnemiesInRange(p.Position(), 300.0f) > 0 && m_E2cd < 0.5f) {
                    m_R.Cast();
                }
            }
        } else if (Combo() && GetBool("autoR")) {
            auto* ts = SDK::TargetSelector::Instance();
            auto t = ts ? ts->GetTarget(1400.0f, DamageType::Physical) : AIHeroClient();
            if (t.IsValid() &&
                !SDK::Extensions::IsValidTarget(t, m_Q1.Range + 200.0f, true) &&
                m_Q.GetDamage(t) * 1.4f > t.Health() &&
                m_Qcd < 0.5f && m_Ecd < 0.5f) {
                m_R.Cast();
            }
            if (!m_Q.IsReady() && (!m_E.IsReady() || GetBool("autoEks"))) {
                m_R.Cast();
            }
        }
    }

    void LaneClearLogic() {
        if (!LaneClear()) return;
        const auto p = Player();

        if (IsRange() && m_Q.IsReady() && m_E.IsReady() && FarmSpells() && GetBool("farmQ")) {
            auto minions = OktwCommon::GetMinions(p.ServerPosition(), m_Q.Range);
            std::vector<SDK::AIBaseClient> baseList(minions.begin(), minions.end());
            auto farm = m_QextCol.GetCircularFarmLocation(baseList, 150.0f);
            if (farm.MinionsHit >= FarmMinions()) m_Q.Cast(farm.Position);
        }

        if (m_W.IsReady() && FarmSpells() && GetBool("farmW")) {
            if (IsRange()) {
                auto mobs = OktwCommon::GetMinions(p.ServerPosition(), 550.0f);
                if (static_cast<int>(mobs.size()) >= FarmMinions()) m_W.Cast();
            } else {
                auto mobs = OktwCommon::GetMinions(p.ServerPosition(), 300.0f);
                if (static_cast<int>(mobs.size()) >= FarmMinions()) m_W.Cast();
            }
        }
    }

    void Jungle() {
        if (!LaneClear()) return;
        const auto p = Player();
        if (p.Mana() <= m_RMANA + m_WMANA + m_WMANA) return;
        auto mobs = OktwCommon::GetMinions(p.ServerPosition(), 700.0f, false, true);
        if (mobs.empty()) return;
        const auto& mob = mobs.front();

        if (IsRange()) {
            if (m_Q.IsReady() && GetBool("jungleQ")) { m_Q.Cast(mob.ServerPosition()); return; }
            if (m_W.IsReady() && GetBool("jungleE")) {
                if (SDK::Extensions::IsValidTarget(mob, 550.0f, true)) m_W.Cast();
                return;
            }
            if (GetBool("jungleR")) m_R.Cast();
        } else {
            if (m_Q1.IsReady() && GetBool("jungleQm") &&
                SDK::Extensions::IsValidTarget(mob, m_Q1.Range, true)) {
                m_Q1.Cast(mob);
                return;
            }
            if (m_W1.IsReady() && GetBool("jungleWm")) {
                if (SDK::Extensions::IsValidTarget(mob, 300.0f, true)) m_W.Cast();
                return;
            }
            if (m_E1.IsReady() && GetBool("jungleEm") &&
                SDK::Extensions::IsValidTarget(mob, m_E1.Range, true)) {
                m_E1.Cast(mob);
                return;
            }
            if (GetBool("jungleR")) m_R.Cast();
        }
    }

    float SetPlus(float v) const { return v < 0.0f ? 0.0f : v; }

    void SetValue() {
        // Approximates C# CooldownExpires tracking. SDK exposes IsReady() but not
        // expiry timestamps directly; use rough proxies (0 when ready).
        if (IsRange()) {
            m_Qcd  = m_Q.IsReady() ? 0.0f : 1.0f;
            m_Wcd  = m_W.IsReady() ? 0.0f : 1.0f;
            m_Ecd  = m_E.IsReady() ? 0.0f : 1.0f;
            m_QMANA = m_Q.Instance().ManaCost();
            m_WMANA = m_W.Instance().ManaCost();
            m_EMANA = m_E.Instance().ManaCost();
        } else {
            m_Q2cd = m_Q.IsReady() ? 0.0f : 1.0f;
            m_W2cd = m_W.IsReady() ? 0.0f : 1.0f;
            m_E2cd = m_E.IsReady() ? 0.0f : 1.0f;
            m_QMANA2 = m_Q.Instance().ManaCost();
            m_WMANA2 = m_W.Instance().ManaCost();
            m_EMANA2 = m_E.Instance().ManaCost();
        }
    }

    void OnGameDraw() override {}
};

} } // namespace Plugins::OKTW
