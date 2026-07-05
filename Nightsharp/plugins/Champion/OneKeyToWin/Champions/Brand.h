#pragma once
// Port of OKTW_CSharp/Champions/Brand.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;

class BrandPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Brand"; }
    const char* GetInternalId() const override { return "champion.oktw.brand"; }
    const char* GetChampionName() const override { return "Brand"; }

protected:
    void BuildMenu() override {
        MarkActive();

        m_Q = Spell(SpellSlot::Q, 1000.0f);
        m_W = Spell(SpellSlot::W, 940.0f);
        m_E = Spell(SpellSlot::E, 625.0f);
        m_R = Spell(SpellSlot::R, 750.0f);

        m_Q.SetSkillshot(0.25f, 60.0f, 1600.0f, true, SDK::SpellType::Line);
        m_W.SetSkillshot(1.15f, 230.0f, FLT_MAX, false, SDK::SpellType::Circle);
        m_R.SetTargetted(0.25f, 2000.0f);

        // Draw
        m_drawMenu->Add(new MenuBool("noti",    "Show notification & line", true));
        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw only ready spells",   true));
        m_drawMenu->Add(new MenuBool("qRange",  "Q range", false));
        m_drawMenu->Add(new MenuBool("wRange",  "W range", false));
        m_drawMenu->Add(new MenuBool("eRange",  "E range", false));
        m_drawMenu->Add(new MenuBool("rRange",  "R range", false));

        // Q Config
        m_qMenu->Add(new MenuBool("autoQ",    "Auto Q", true));
        m_qMenu->Add(new MenuBool("QAblazed", "Q only if ablazed", true));
        m_qMenu->Add(new MenuBool("harassQ",  "Harass Q", true));
        m_qMenu->Add(new MenuBool("gapQ",     "Gapcloser E + Q", true));
        m_qMenu->Add(new MenuBool("intQ",     "Interrupt spells E + Q", true));

        // W Config
        m_wMenu->Add(new MenuBool("autoW",   "Auto W", true));
        m_wMenu->Add(new MenuBool("harassW", "Harass W", true));

        // E Config
        m_eMenu->Add(new MenuBool("autoE",   "Auto E", true));
        m_eMenu->Add(new MenuBool("harassE", "Harass E", true));
        m_eMenu->Add(new MenuBool("minionE", "use E on ablazed minion", true));

        // R Config
        m_rMenu->Add(new MenuBool("autoR",   "Auto R", true));
        m_rMenu->Add(new MenuSlider("rCount", "Auto R if can hit x enemies", 3, 0, 5));

        // Farm
        m_farmMenu->Add(new MenuBool("farmE",    "Lane clear E", true));
        m_farmMenu->Add(new MenuBool("farmW",    "Lane clear W", true));
        m_farmMenu->Add(new MenuBool("jungleE",  "Jungle clear E", true));
        m_farmMenu->Add(new MenuBool("jungleQ",  "Jungle clear Q", true));
        m_farmMenu->Add(new MenuBool("jungleW",  "Jungle clear W", true));

        // NOTE: AntiGapcloser + Interrupter callbacks are not wired here — SDK
        // hooks for gap-closer / interruptible spells are TODO. The gapQ/intQ
        // menu items are exposed to match the C# parity; the wiring will slot
        // into place when those SDK events land. See AntiGapcloser_OnEnemyGapcloser
        // and Interrupter2_OnInterruptableTarget below for the intended logic.
    }

    // ----------------------------------------------------------------------
    // Anti-gapcloser handler (C#: AntiGapcloser_OnEnemyGapcloser)
    // TODO: hook this up when SDK gap-closer events become available.
    void AntiGapcloser_OnEnemyGapcloser(const AIHeroClient& sender) {
        if (!GetBool("gapQ") || Player().Mana() < m_QMANA + m_EMANA) return;

        if (SDK::Extensions::IsValidTarget(sender, m_E.Range, true) &&
            (sender.HasBuff("brandablaze") || m_E.IsReady())) {
            m_E.CastOnUnit(sender);
            if (m_Q.IsReady()) m_Q.Cast(sender);
        }
    }

    // Interrupter handler (C#: Interrupter2_OnInterruptableTarget)
    // TODO: hook this up when SDK interrupter events become available.
    void Interrupter2_OnInterruptableTarget(const AIHeroClient& t) {
        if (!GetBool("intQ") || Player().Mana() < m_QMANA + m_EMANA) return;

        if (SDK::Extensions::IsValidTarget(t, m_E.Range, true) &&
            (t.HasBuff("brandablaze") || m_E.IsReady())) {
            m_E.CastOnUnit(t);
            if (m_Q.IsReady()) m_Q.Cast(t);
        }
    }

    // ----------------------------------------------------------------------
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
            // Match C#: RMANA = QMANA - PARRegenRate * Q.Cooldown
            // TODO(oktw-port): PARRegenRate unavailable
            m_RMANA = m_QMANA - 0.0f * m_Q.Instance().Cooldown();
        } else {
            m_RMANA = m_R.Instance().ManaCost();
        }
    }

    void OnGameUpdate() override {
        // Orbwalker attack gate mirrors C#: block AA in combo while E is up
        // (kept as commented parity marker — SDK::Orbwalker::SetAttack not
        // exposed here; the shared orbwalker handles it via Combo state).
        // if (Combo()) { SDK::Orbwalker::SetAttack(!m_E.IsReady()); }
        // else         { SDK::Orbwalker::SetAttack(true); }

        if (LagFree(0)) {
            SetMana();
            Jungle();
        }

        if (LagFree(1) && m_E.IsReady() && GetBool("autoE")) LogicE();
        if (LagFree(2) && m_Q.IsReady() && GetBool("autoQ")) LogicQ();
        if (LagFree(3) && m_W.IsReady() && GetBool("autoW")) LogicW();
        if (LagFree(4) && m_R.IsReady() && GetBool("autoR")) LogicR();
    }

    // ----------------------------------------------------------------------
    void LogicQ() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_Q.Range, SDK::DamageType::Magical) : AIHeroClient();
        if (!t.IsValid()) return;

        const auto p = Player();

        // KS: Q + BonusDmg + EchoLuden > Health
        if (OktwCommon::GetKsDamage(t, m_Q) + BonusDmg(t) + OktwCommon::GetEchoLudenDamage(t) > t.Health()) {
            CastSpell(m_Q, t);
        }

        // "Q only if ablazed" — if target isn't ablazed, prefer switching to an ablazed enemy
        if (!t.HasBuff("brandablaze") && GetBool("QAblazed")) {
            AIHeroClient otherEnemy = t;
            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                if (SDK::Extensions::IsValidTarget(enemy, m_Q.Range, true) &&
                    enemy.HasBuff("brandablaze")) {
                    t = enemy;
                }
            }
            // Same target still — bail unless our fallback logic says otherwise
            if (otherEnemy.NetworkId() == t.NetworkId() && !LogicQuse(t)) return;
        }

        if (Combo() && p.Mana() > m_RMANA + m_QMANA) {
            CastSpell(m_Q, t);
        } else if (Harass() && GetBool("harassQ") &&
                   p.Mana() > m_RMANA + m_EMANA + m_WMANA + m_EMANA &&
                   GetBool((std::string("Harass") + t.CharacterName()).c_str())) {
            CastSpell(m_Q, t);
        }

        // Immobile enemies within range
        if (p.Mana() > m_RMANA + m_QMANA) {
            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                if (SDK::Extensions::IsValidTarget(enemy, m_Q.Range, true) &&
                    !OktwCommon::CanMove(enemy)) {
                    m_Q.Cast(enemy);
                }
            }
        }
    }

    bool LogicQuse(const AIBaseClient& t) {
        if (t.HasBuff("brandablaze")) return true;

        const float gameTime = SDK::Game::Time();
        const float qCd      = m_Q.Instance().Cooldown();
        const float eLeft    = m_E.Instance().CooldownExpires() - gameTime;
        const float wLeft    = m_W.Instance().CooldownExpires() - gameTime;

        return (eLeft + 2.0f >= qCd) && (wLeft + 2.0f >= qCd);
    }

    // ----------------------------------------------------------------------
    void LogicW() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_W.Range, SDK::DamageType::Magical) : AIHeroClient();
        const auto p = Player();

        if (t.IsValid()) {
            if (Combo() && p.Mana() > m_RMANA + m_WMANA) {
                CastSpell(m_W, t);
            } else if (Harass() && GetBool("harassW") &&
                       GetBool((std::string("Harass") + t.CharacterName()).c_str()) &&
                       p.Mana() > m_RMANA + m_WMANA + m_EMANA + m_QMANA + m_WMANA &&
                       OktwCommon::CanHarras()) {
                CastSpell(m_W, t);
            } else {
                const float qDmg = m_Q.GetDamage(t);
                const float wDmg = OktwCommon::GetKsDamage(t, m_W) + BonusDmg(t);
                if (wDmg > t.Health()) {
                    CastSpell(m_W, t);
                } else if (wDmg + qDmg > t.Health() && p.Mana() > m_QMANA + m_EMANA) {
                    CastSpell(m_W, t);
                }
            }

            // Immobile enemies
            if (p.Mana() > m_RMANA + m_WMANA) {
                for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                    if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                    if (SDK::Extensions::IsValidTarget(enemy, m_W.Range, true) &&
                        !OktwCommon::CanMove(enemy)) {
                        m_W.Cast(enemy);
                    }
                }
            }
        } else if (FarmSpells() && GetBool("farmW")) {
            auto minions = OktwCommon::GetMinions(Player().ServerPosition(), m_W.Range);
            std::vector<SDK::AIBaseClient> baseList(minions.begin(), minions.end());
            auto farm = m_W.GetCircularFarmLocation(baseList, m_W.Width);
            if (farm.MinionsHit >= FarmMinions()) m_W.Cast(farm.Position);
        }
    }

    // ----------------------------------------------------------------------
    void LogicE() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_E.Range, SDK::DamageType::Magical) : AIHeroClient();
        const auto p = Player();

        if (t.IsValid()) {
            if (Combo() && p.Mana() > m_RMANA + m_EMANA) {
                m_E.CastOnUnit(t);
            } else if (Harass() && GetBool("harassE") &&
                       p.Mana() > m_RMANA + m_EMANA + m_WMANA + m_EMANA &&
                       GetBool((std::string("Harass") + t.CharacterName()).c_str())) {
                m_E.CastOnUnit(t);
            } else {
                const float eDmg = OktwCommon::GetKsDamage(t, m_E) + BonusDmg(t) + OktwCommon::GetEchoLudenDamage(t);
                const float wDmg = m_W.GetDamage(t);
                if (eDmg > t.Health()) {
                    m_E.CastOnUnit(t);
                } else if (wDmg + eDmg > t.Health() && p.Mana() > m_WMANA + m_EMANA) {
                    m_E.CastOnUnit(t);
                }
            }
        } else {
            if (!GetBool("minionE")) return;

            // Ablazed-minion pickups near enemies
            if ((Combo() && p.Mana() > m_RMANA + m_EMANA) ||
                (Harass() && GetBool("harassE") && p.Mana() > m_RMANA + m_EMANA)) {
                auto minions = OktwCommon::GetMinions(p.Position(), m_E.Range);
                for (const auto& minion : minions) {
                    if (SDK::Extensions::IsValidTarget(minion, m_E.Range, false) &&
                        OktwCommon::CountEnemiesInRange(minion.Position(), 300.0f) > 0 &&
                        minion.HasBuff("brandablaze")) {
                        m_E.CastOnUnit(minion);
                    }
                }
            }

            // Lane-clear on ablazed minions clustering
            if (FarmSpells() && GetBool("farmE") && p.Mana() > m_RMANA + m_EMANA) {
                auto minions = OktwCommon::GetMinions(p.Position(), m_E.Range);
                for (const auto& minion : minions) {
                    if (SDK::Extensions::IsValidTarget(minion, m_E.Range, false) &&
                        minion.HasBuff("brandablaze") &&
                        CountMinionsInRange(400.0f, minion.Position()) >= FarmMinions()) {
                        m_E.CastOnUnit(minion);
                    }
                }
            }
        }
    }

    // ----------------------------------------------------------------------
    void LogicR() {
        const float bounceRange = 430.0f;

        auto* ts = SDK::TargetSelector::Instance();
        auto t2 = ts ? ts->GetTarget(m_R.Range + bounceRange, SDK::DamageType::Magical) : AIHeroClient();

        const int rCount = GetSlider("rCount", 3);

        if (SDK::Extensions::IsValidTarget(t2, m_R.Range, true) &&
            OktwCommon::CountEnemiesInRange(t2.Position(), bounceRange) >= rCount &&
            rCount > 0) {
            m_R.Cast(t2);
        }

        if (!t2.IsValid() || !OktwCommon::ValidUlt(t2)) return;

        const auto player = Player();
        if (OktwCommon::CountAlliesInRange(t2.Position(), 550.0f) != 0 &&
            player.HealthPercent() >= 50.0f &&
            OktwCommon::CountEnemiesInRange(t2.Position(), bounceRange) <= 1) {
            return;
        }

        const auto pred = m_R.GetPrediction(t2);
        const Vector3 prepos = pred.GetCastPosition();
        const float dmgR = m_R.GetDamage(t2);

        if (t2.Health() >= dmgR * 3.0f) return;

        float totalDmg = dmgR;
        const int minionCount = CountMinionsInRange(bounceRange, prepos);

        if (SDK::Extensions::IsValidTarget(t2, m_R.Range, true)) {
            if (OktwCommon::CountEnemiesInRange(prepos, bounceRange) > 1) {
                totalDmg = (minionCount > 2) ? dmgR * 2.0f : dmgR * 3.0f;
            } else if (minionCount > 0) {
                totalDmg = dmgR * 2.0f;
            }

            if (m_W.IsReady()) totalDmg += m_W.GetDamage(t2);
            if (m_E.IsReady()) totalDmg += m_E.GetDamage(t2);
            if (m_Q.IsReady()) totalDmg += m_Q.GetDamage(t2);

            totalDmg += BonusDmg(t2);
            totalDmg += OktwCommon::GetEchoLudenDamage(t2);

            // Zhonya's Hourglass (3157) and Guardian Angel-like items in C# used
            // item ids 3155/3156; kept as TODO — SDK Items lookup by owner not
            // wired here.
            // TODO: subtract 250 if target has item 3155
            // TODO: subtract 400 if target has item 3156

            if (totalDmg > t2.Health() - OktwCommon::GetIncomingDamage(t2) &&
                player.GetAutoAttackDamage(t2, true) * 2.0f < t2.Health()) {
                m_R.CastOnUnit(t2);
            }
        } else if (t2.Health() - OktwCommon::GetIncomingDamage(t2) < dmgR * 2.0f + BonusDmg(t2)) {
            if (OktwCommon::CountEnemiesInRange(player.Position(), m_R.Range) > 0) {
                for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                    if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                    if (SDK::Extensions::IsValidTarget(enemy, m_R.Range, true) &&
                        enemy.Position().Distance(prepos) < bounceRange) {
                        m_R.CastOnUnit(enemy);
                    }
                }
            } else {
                auto minions = OktwCommon::GetMinions(player.Position(), m_R.Range);
                for (const auto& minion : minions) {
                    if (SDK::Extensions::IsValidTarget(minion, m_R.Range, false) &&
                        minion.Position().Distance(prepos) < bounceRange) {
                        m_R.CastOnUnit(minion);
                    }
                }
            }
        }
    }

    // ----------------------------------------------------------------------
    void Jungle() {
        if (!LaneClear()) return;
        const auto p = Player();
        if (p.Mana() <= m_RMANA + m_WMANA + m_RMANA + m_WMANA) return;

        auto mobs = OktwCommon::GetMinions(p.ServerPosition(), 600.0f, false, true);
        if (mobs.empty()) return;

        const auto& mob = mobs.front();

        if (m_W.IsReady() && GetBool("jungleW")) {
            m_W.Cast(mob.ServerPosition());
            return;
        }
        if (m_Q.IsReady() && GetBool("jungleQ")) {
            m_Q.Cast(mob.ServerPosition());
            return;
        }
        if (m_E.IsReady() && GetBool("jungleE") && mob.HasBuff("brandablaze")) {
            m_E.CastOnUnit(mob);
            return;
        }
    }

    // ----------------------------------------------------------------------
    int CountMinionsInRange(float range, const Vector3& pos) {
        auto minions = OktwCommon::GetMinions(pos, range);
        return static_cast<int>(minions.size());
    }

    float BonusDmg(const AIHeroClient& target) {
        // C#: Player.CalcDamage(target, Magical, MaxHealth*0.08 - HPRegenRate*5)
        const float raw = target.MaxHealth() * 0.08f - 0.0f * 5.0f; // TODO(oktw-port): HPRegenRate() not available in SDK
        // TODO: SDK-side magic-resist-aware CalcDamage; return raw as best-effort
        return raw > 0.0f ? raw : 0.0f;
    }

    // ----------------------------------------------------------------------
    void OnGameDraw() override {
        // Simplified: rely on SDK draw utilities for range circles. C# also
        // rendered a "N x Ult can kill" notification + line — TODO once SDK
        // text draw APIs are threaded in.
    }
};

} } // namespace Plugins::OKTW
