#pragma once
// Port of OKTW_CSharp/Champions/Xerath.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;
using SDK::UI::MenuKeyBind;

class XerathPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Xerath"; }
    const char* GetInternalId() const override { return "champion.oktw.xerath"; }
    const char* GetChampionName() const override { return "Xerath"; }

protected:
    Vector3 m_Rtarget{};
    float   m_lastR = 0.0f;

    // Charged-Q state — driven directly via Spellbook().UpdateChargedSpell,
    // mirroring the working XerathSemiPlugin. The wrapper's IsCharging()/Cast()
    // charged path is bypassed because the Q slot reports "not ready" while
    // charging, which would gate LogicQ out and leave the charge never released.
    bool    m_qCharging      = false;
    int     m_qChargeStartTick = 0;
    int     m_qChargeReqTick   = 0;
    Vector3 m_qLastPos{};   // last predicted release position (backstop if target lost)

    static constexpr int   kQChargeDurationMs = 1500;
    static constexpr int   kQMinHoldMs = 120;   // let range build before releasing
    static constexpr float kQMinRange = 750.0f;
    static constexpr float kQMaxRange = 1550.0f;

    void BuildMenu() override {
        MarkActive();

        m_Q = Spell(SpellSlot::Q, 1550.0f);
        m_W = Spell(SpellSlot::W, 1100.0f);
        m_E = Spell(SpellSlot::E, 1050.0f);
        m_R = Spell(SpellSlot::R, 675.0f);

        m_Q.SetSkillshot(0.6f, 95.0f,  FLT_MAX, false, SDK::SpellType::Line);
        m_W.SetSkillshot(0.7f, 125.0f, FLT_MAX, false, SDK::SpellType::Circle);
        m_E.SetSkillshot(0.25f, 60.0f, 1400.0f, true,  SDK::SpellType::Line);
        m_R.SetSkillshot(0.7f, 130.0f, FLT_MAX, false, SDK::SpellType::Circle);

        // Q is a charged spell
        m_Q.SetCharged("XerathArcanopulseChargeUp", "XerathArcanopulseChargeUp", 750, 1550, 1.5f);

        m_drawMenu->Add(new MenuBool("noti",       "Show notification & line", true));
        m_drawMenu->Add(new MenuBool("onlyRdy",    "Draw only ready spells", true));
        m_drawMenu->Add(new MenuBool("qRange",     "Q range", false));
        m_drawMenu->Add(new MenuBool("wRange",     "W range", false));
        m_drawMenu->Add(new MenuBool("eRange",     "E range", false));
        m_drawMenu->Add(new MenuBool("rRange",     "R range", false));
        m_drawMenu->Add(new MenuBool("rRangeMini", "R range minimap", true));

        m_qMenu->Add(new MenuBool("autoQ",   "Auto Q", true));
        m_qMenu->Add(new MenuBool("harassQ", "harass Q", true));

        m_wMenu->Add(new MenuBool("autoW",   "Auto W", true));
        m_wMenu->Add(new MenuBool("harassW", "Harass W", true));

        m_eMenu->Add(new MenuBool("autoE",   "Auto E", true));
        m_eMenu->Add(new MenuBool("harassE", "Harass E", true));

        m_rMenu->Add(new MenuBool("autoR",     "Auto R 2 x dmg R", true));
        m_rMenu->Add(new MenuBool("autoRlast", "Cast last position if no target", true));
        m_rMenu->Add(new MenuKeyBind("useR",   "Semi-manual cast R key", 'T', SDK::KeyBindType::Press));
        m_rMenu->Add(new MenuBool("trinkiet",  "Auto blue trinkiet", true));
        m_rMenu->Add(new MenuSlider("delayR",     "custome R delay ms (1000ms = 1 sec)", 0, 0, 3000));
        m_rMenu->Add(new MenuSlider("MaxRangeR",  "Max R adjustment (R range - slider)", 0, 0, 5000));

        m_farmMenu->Add(new MenuBool("separate", "Separate laneclear from harras", false));
        m_farmMenu->Add(new MenuBool("farmQ",    "Lane clear Q", true));
        m_farmMenu->Add(new MenuBool("farmW",    "Lane clear W", true));
        m_farmMenu->Add(new MenuBool("jungleE",  "Jungle clear E", true));
        m_farmMenu->Add(new MenuBool("jungleQ",  "Jungle clear Q", true));
        m_farmMenu->Add(new MenuBool("jungleW",  "Jungle clear W", true));

        m_champMenu->Add(new MenuBool("force", "Force passive use in combo on minion", true));

        // TODO(oktw-port): Interrupter2, AntiGapcloser, Spellbook.OnCastSpell,
        //                  Obj_AI_Base.OnProcessSpellCast, BeforeAttack, AfterAttack,
        //                  Player.OnIssueOrder events not available
    }

    bool IsCastingR() const {
        return Player().HasBuff("XerathLocusOfPower2");
    }

    // ── Charged-Q control (direct spellbook drive, like XerathSemiPlugin) ──
    bool IsQCharging() const {
        return Player().HasBuff("XerathArcanopulseChargeUp") || m_qCharging;
    }

    float QCurrentRange() const {
        if (!IsQCharging()) return kQMaxRange;
        const int elapsed = std::max(0, SDK::Game::TickCount() - m_qChargeStartTick);
        const float progress = std::min(1.0f,
            static_cast<float>(elapsed) / static_cast<float>(kQChargeDurationMs));
        return kQMinRange + (kQMaxRange - kQMinRange) * progress;
    }

    void BeginQCharge(const Vector3& position) {
        if (IsQCharging()) return;
        if (SDK::Game::TickCount() - m_qChargeReqTick <= 400 + SDK::Game::Ping()) return;
        SDK::ObjectManager::Player().Spellbook().UpdateChargedSpell(
            SpellSlot::Q, position, false);
        m_qCharging = true;
        m_qChargeStartTick = SDK::Game::TickCount();
        m_qChargeReqTick = SDK::Game::TickCount();
        m_qLastPos = position;
    }

    void ReleaseQ(const Vector3& position) {
        SDK::ObjectManager::Player().Spellbook().UpdateChargedSpell(
            SpellSlot::Q, position, true);
        m_qCharging = false;
        m_qChargeStartTick = 0;
    }

    void SetMana() override {
        if ((Shared().manaDisable && Shared().manaDisable->Value && Combo()) ||
            Player().HealthPercent() < 20.0f || IsQCharging()) {
            m_QMANA = m_WMANA = m_EMANA = m_RMANA = 0.0f;
            return;
        }
        m_QMANA = m_Q.Instance().ManaCost();
        m_WMANA = m_W.Instance().ManaCost();
        m_EMANA = m_E.Instance().ManaCost();
        m_RMANA = m_R.IsReady() ? m_R.Instance().ManaCost() : m_QMANA;
    }

    void OnGameUpdate() override {
        if (LagFree(3) && m_R.IsReady()) LogicR();

        if (IsCastingR()) return;

        if (LagFree(1)) {
            SetMana();
            Jungle();
        }

        if (m_E.IsReady() && GetBool("autoE")) LogicE();
        if (LagFree(2) && m_W.IsReady() && GetBool("autoW")) LogicW();
        // Q must run while charging too — the slot reports "not ready" mid-charge,
        // so gating on m_Q.IsReady() alone would leave the charge never released.
        if (LagFree(4) && (m_Q.IsReady() || IsQCharging()) && GetBool("autoQ")) LogicQ();
    }

    void LogicR() {
        // Range grows with level - use base range plus level bonus
        const int rLevel = m_R.Level();
        m_R.Range = 2000.0f + rLevel * 1200.0f;
        if (!IsCastingR())
            m_R.Range = m_R.Range - GetSlider("MaxRangeR", 0);

        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_R.Range, DamageType::Magical) : AIHeroClient();
        const auto p = Player();
        if (t.IsValid()) {
            if (GetKey("useR") && !IsCastingR()) {
                m_R.Cast();
            }
            if (!SDK::Extensions::IsValidTarget(t, m_W.Range, true) && GetBool("autoR") &&
                !IsCastingR() &&
                OktwCommon::CountAlliesInRange(t.Position(), 500.0f) == 0 &&
                OktwCommon::CountEnemiesInRange(p.Position(), 1100.0f) == 0) {
                if (OktwCommon::GetKsDamage(t, m_R) + (m_R.GetDamage(t) * rLevel) > t.Health()) {
                    m_R.Cast();
                }
            }
            if (IsCastingR()) {
                CastSpell(m_R, t);
            }
            m_Rtarget = m_R.GetPrediction(t, true).GetCastPosition();
        } else if (GetBool("autoRlast") && IsCastingR()) {
            m_R.Cast(m_Rtarget);
        }
    }

    void LogicW() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_W.Range, DamageType::Physical) : AIHeroClient();
        const auto p = Player();
        if (t.IsValid()) {
            const float qDmg = m_Q.GetDamage(t);
            const float wDmg = OktwCommon::GetKsDamage(t, m_W);

            if (wDmg > t.Health()) {
                CastSpell(m_W, t);
            } else if (wDmg + qDmg > t.Health() && p.Mana() > m_WMANA + m_QMANA) {
                CastSpell(m_W, t);
            } else if (Combo() && p.Mana() > m_RMANA + m_WMANA) {
                CastSpell(m_W, t);
            } else if (Harass() && OktwCommon::CanHarras() && GetBool("harassW") &&
                       GetBool((std::string("Harass") + t.CharacterName()).c_str()) &&
                       p.Mana() > m_RMANA + m_WMANA + m_EMANA + m_QMANA + m_WMANA) {
                CastSpell(m_W, t);
            } else if (Combo() || Harass()) {
                for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                    if (enemy.IsValid() && enemy.IsEnemy() &&
                        SDK::Extensions::IsValidTarget(enemy, m_W.Range, true) &&
                        !OktwCommon::CanMove(enemy)) {
                        m_W.Cast(enemy);
                    }
                }
            }
        } else if (FarmSpells() && GetBool("farmW") && p.Mana() > m_RMANA + m_WMANA) {
            auto minions = OktwCommon::GetMinions(p.ServerPosition(), m_W.Range);
            std::vector<SDK::AIBaseClient> baseList(minions.begin(), minions.end());
            auto farm = m_W.GetCircularFarmLocation(baseList, m_W.Width);
            if (farm.MinionsHit >= FarmMinions()) m_W.Cast(farm.Position);
        }
    }

    void LogicQ() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t  = ts ? ts->GetTarget(m_Q.Range, DamageType::Magical) : AIHeroClient();
        auto t2 = ts ? ts->GetTarget(1500.0f,   DamageType::Magical) : AIHeroClient();
        const auto p = Player();

        const bool separateLC = GetBool("separate") && LaneClear();

        // Backstop: if we lost the target mid-charge, don't hold forever. Once the
        // charge is full, release at the last known position so the charge isn't wasted.
        if (IsQCharging() && !t.IsValid()) {
            const int elapsed = std::max(0, SDK::Game::TickCount() - m_qChargeStartTick);
            if (elapsed >= kQChargeDurationMs && m_qLastPos.IsValid() && !m_qLastPos.IsZero())
                ReleaseQ(m_qLastPos);
            return;
        }

        if (t.IsValid() && t2.IsValid() && t.NetworkId() == t2.NetworkId() && !separateLC) {
            if (IsQCharging()) {
                // Release escalation driven by elapsed charge time (robust against
                // the charge buff not yet being present in the first ticks — using
                // the buff's remaining time here would release instantly at min range).
                const int elapsed = std::max(0, SDK::Game::TickCount() - m_qChargeStartTick);
                if (elapsed < kQMinHoldMs) return;  // let range build first

                const auto pred = m_Q.GetPrediction(t, true);
                const Vector3 castPos = pred.GetCastPosition();
                m_qLastPos = castPos;
                const bool fullyCharged = elapsed >= kQChargeDurationMs;

                if (fullyCharged ||
                    OktwCommon::CountEnemiesInRange(p.Position(), 800.0f) > 0 ||
                    p.Position().Distance(t.Position()) > 1450.0f) {
                    ReleaseQ(castPos);
                } else if (elapsed >= kQChargeDurationMs / 2 ||
                           OktwCommon::CountEnemiesInRange(p.Position(), 1000.0f) > 0) {
                    if (static_cast<int>(pred.Hitchance) >= static_cast<int>(SDK::HitChance::VeryHigh))
                        ReleaseQ(castPos);
                }
                return;
            } else if (SDK::Extensions::IsValidTarget(t, m_Q.Range - 300.0f, true)) {
                const Vector3 chargePos = m_Q.GetPrediction(t, true).GetCastPosition();
                if (t.Health() < OktwCommon::GetKsDamage(t, m_Q)) {
                    BeginQCharge(chargePos);
                } else if (Combo() && p.Mana() > m_EMANA + m_QMANA) {
                    BeginQCharge(chargePos);
                } else if (Harass() && SDK::Extensions::IsValidTarget(t, 1200.0f, true) &&
                           GetBool("harassQ") &&
                           p.Mana() > m_RMANA + m_EMANA + m_QMANA + m_QMANA &&
                           GetBool((std::string("Harass") + t.CharacterName()).c_str()) &&
                           OktwCommon::CanHarras()) {
                    BeginQCharge(chargePos);
                } else if ((Combo() || Harass()) && p.Mana() > m_RMANA + m_WMANA) {
                    for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                        if (enemy.IsValid() && enemy.IsEnemy() &&
                            SDK::Extensions::IsValidTarget(enemy, m_Q.Range, true) &&
                            !OktwCommon::CanMove(enemy)) {
                            BeginQCharge(m_Q.GetPrediction(enemy, true).GetCastPosition());
                            break;
                        }
                    }
                }
            }
        } else if (LaneClear() && m_Q.Range > 1000.0f &&
                   OktwCommon::CountEnemiesInRange(p.Position(), 1450.0f) == 0 &&
                   (IsQCharging() || (FarmSpells() && GetBool("farmQ")))) {
            auto minions = OktwCommon::GetMinions(p.ServerPosition(), m_Q.Range);
            std::vector<SDK::AIBaseClient> baseList(minions.begin(), minions.end());
            auto farm = m_Q.GetLineFarmLocation(baseList, m_Q.Width);
            const Vector3 farmPos = Vector3::From2D(farm.Position);
            if (!IsQCharging()) {
                if (farm.MinionsHit >= FarmMinions()) BeginQCharge(farmPos);
            } else {
                const int elapsed = std::max(0, SDK::Game::TickCount() - m_qChargeStartTick);
                if (elapsed >= kQMinHoldMs && farm.MinionsHit > 0)
                    ReleaseQ(farmPos);
            }
        }
    }

    void LogicE() {
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(enemy, m_E.Range, true)) continue;
            const float total = m_E.GetDamage(enemy) +
                                OktwCommon::GetKsDamage(enemy, m_Q) +
                                m_W.GetDamage(enemy) +
                                OktwCommon::GetEchoLudenDamage(enemy);
            if (total > enemy.Health()) {
                CastSpell(m_E, enemy);
            }
        }

        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_E.Range, DamageType::Magical) : AIHeroClient();
        const auto p = Player();
        if (t.IsValid()) {
            if (Combo() && p.Mana() > m_RMANA + m_EMANA)
                CastSpell(m_E, t);
            if (Harass() && OktwCommon::CanHarras() && GetBool("harassE") &&
                p.Mana() > m_RMANA + m_EMANA + m_WMANA + m_EMANA)
                CastSpell(m_E, t);
            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (enemy.IsValid() && enemy.IsEnemy() &&
                    SDK::Extensions::IsValidTarget(enemy, m_E.Range, true) &&
                    !OktwCommon::CanMove(enemy)) {
                    m_E.Cast(enemy);
                }
            }
        }
    }

    void Jungle() {
        if (!LaneClear()) return;
        const auto p = Player();
        if (p.Mana() <= m_RMANA + m_WMANA + m_RMANA + m_WMANA) return;
        auto mobs = OktwCommon::GetMinions(p.ServerPosition(), 600.0f, false, true);
        if (mobs.empty()) return;
        const auto& mob = mobs.front();
        if (m_E.IsReady() && GetBool("jungleE")) { m_E.Cast(mob.ServerPosition()); return; }
        if (m_W.IsReady() && GetBool("jungleW")) { m_W.Cast(mob.ServerPosition()); return; }
        if (m_Q.IsReady() && GetBool("jungleQ")) { m_Q.Cast(mob.ServerPosition()); return; }
    }

    void OnGameDraw() override {}
};

} } // namespace Plugins::OKTW
