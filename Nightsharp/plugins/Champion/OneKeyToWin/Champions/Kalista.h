#pragma once
// Port of OKTW_CSharp/Champions/Kalista.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;

class KalistaPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Kalista"; }
    const char* GetInternalId() const override { return "champion.oktw.kalista"; }
    const char* GetChampionName() const override { return "Kalista"; }

protected:
    // ── Runtime state (mirrors C# private fields) ──
    Spell        m_Q1{ SpellSlot::Q };  // non-collision Q variant
    int          m_wCount = 0;
    float        m_grabTime = 0.0f;
    float        m_lastECast = 0.0f;
    AIHeroClient m_AllyR{};
    bool         m_hasAllyR = false;

    void BuildMenu() override {
        MarkActive();

        m_Q  = Spell(SpellSlot::Q, 1170.0f);
        m_Q1 = Spell(SpellSlot::Q, 1170.0f);
        m_W  = Spell(SpellSlot::W, 5000.0f);
        m_E  = Spell(SpellSlot::E, 1000.0f);
        m_R  = Spell(SpellSlot::R, 1500.0f);

        m_Q.SetSkillshot(0.10f, 40.0f, 2400.0f, true,  SDK::SpellType::Line);
        m_Q1.SetSkillshot(0.10f, 40.0f, 2400.0f, false, SDK::SpellType::Line);

        m_grabTime = SDK::Game::Time();

        m_drawMenu->Add(new MenuBool("qRange",   "Q range", false));
        m_drawMenu->Add(new MenuBool("eRange",   "E range", false));
        m_drawMenu->Add(new MenuBool("rRange",   "R range", false));
        m_drawMenu->Add(new MenuBool("eDamage",  "E damage %", false));
        m_drawMenu->Add(new MenuBool("onlyRdy",  "Draw only ready spells", true));

        m_qMenu->Add(new MenuSlider("qMana", "Q harass mana %", 50, 0, 100));
        static const char* qModes[] = { "Always", "OKTW logic" };
        m_qMenu->Add(new MenuList("qMode", "Q combo mode", qModes, 2, 1));

        m_eMenu->Add(new MenuSlider("countE",  "Auto E if x stacks", 10, 0, 30));
        m_eMenu->Add(new MenuSlider("Edmg",    "E % dmg adjust", 100, 50, 150));
        m_eMenu->Add(new MenuBool("Edead",     "Cast E before Kalista dead", true));
        m_eMenu->Add(new MenuBool("Ekillmin",  "Cast E minion kill + harras target", true));

        m_wMenu->Add(new MenuBool("autoW",   "Auto W", true));
        m_wMenu->Add(new MenuBool("Wdragon", "Auto W Dragon, Baron, Blue, Red", true));

        // C# put "autoR" directly on the champ submenu, not R Config
        m_champMenu->Add(new MenuBool("autoR", "Auto R", true));

        Menu* balista = m_champMenu->AddSubMenu(new Menu("BalistaConfig", "Balista Config"));
        balista->Add(new MenuBool("balista", "Balista R", true));
        balista->Add(new MenuSlider("rangeBalista", "Balista min range", 300, 0, 1400));

        m_farmMenu->Add(new MenuBool("farmQ",       "Lane clear Q", true));
        m_farmMenu->Add(new MenuBool("farmE",       "Lane clear E", true));
        m_farmMenu->Add(new MenuSlider("farmEcount", "Auto E if x minions", 2, 1, 10));
        m_farmMenu->Add(new MenuSlider("farmQcount", "Lane clear Q if x minions", 2, 1, 10));
        m_farmMenu->Add(new MenuBool("minionE",     "Auto E big minion", true));

        Menu* eSub = m_farmMenu->AddSubMenu(new Menu("EFarmConfig", "E Config"));
        eSub->Add(new MenuBool("jungleE", "Jungle ks E", true));
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
        if (!m_R.IsReady())
            m_RMANA = m_QMANA;  // C# used QMANA - PARRegenRate * Q.Cooldown; approximated
        else
            m_RMANA = m_R.Instance().ManaCost();
    }

    // ── C# GetPassiveTime(Obj_AI_Base) — remaining time on kalistaexpungemarker ──
    float GetPassiveTime(const SDK::AIBaseClient& target) const {
        return OktwCommon::GetPassiveTime(target, "kalistaexpungemarker");
    }

    int GetEStacks(const SDK::AIBaseClient& target) const {
        return OktwCommon::GetBuffCount(target, "kalistaexpungemarker");
    }

    // ── C# GetEdmg ──
    float GetEdmg(const SDK::AIBaseClient& t) const {
        float eDamage = m_E.GetDamage(t);
        const auto p = Player();
        if (p.HasBuff("summonerexhaust")) eDamage *= 0.6f;
        if (t.HasBuff("ferocioushowl"))   eDamage *= 0.7f;

        // AIHeroClient specialization for Blitzcrank
        // (t may be a hero — use dynamic address comparison via IsHero if avail)
        for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
            if (enemy.NetworkId() != t.NetworkId()) continue;
            if (std::string(enemy.CharacterName()) == "Blitzcrank" &&
                !enemy.HasBuff("BlitzcrankManaBarrierCD") &&
                !enemy.HasBuff("ManaBarrier")) {
                eDamage -= enemy.Mana() / 2.0f;
            }
            break;
        }

        eDamage -= 0.0f; // TODO(oktw-port): HPRegenRate() not available in SDK
        eDamage -= 0.0f * 0.005f * t.BonusAttackDamage(); // TODO(oktw-port): PercentLifeStealMod() not available in SDK
        eDamage = eDamage * 0.01f * static_cast<float>(GetSlider("Edmg", 100));
        return eDamage;
    }

    int CountMeleeInRange(float range) const {
        int count = 0;
        for (const auto& target : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!target.IsValid() || !target.IsEnemy()) continue;
            if (SDK::Extensions::IsValidTarget(target, range, true) && target.IsMelee())
                ++count;
        }
        return count;
    }

    void CastE() {
        if (SDK::Game::Time() - m_lastECast < 0.4f) return;
        m_E.Cast();
        m_lastECast = SDK::Game::Time();
    }

    void CastQ(bool cast, const SDK::AIBaseClient& t) {
        if (cast) Plugins::OKTW::CastSpell(m_Q1, t);
        else      Plugins::OKTW::CastSpell(m_Q, t);
    }

    // TODO(oktw-port): AntiGapcloser.OnEnemyGapcloser — no equivalent event yet.
    // TODO(oktw-port): Obj_AI_Base.OnProcessSpellCast — not wired; grabTime and
    // wCount stay in their init state (grabTime = 0, wCount = 0).

    void OnGameUpdate() override {
        const auto p = Player();
        if (!p.IsValid() || p.HasBuff("Recall")) return;

        // Balista R (Blitzcrank grab) — approximated without OnProcessSpellCast
        if (m_R.IsReady() && m_hasAllyR && GetBool("balista") &&
            m_AllyR.IsVisible() &&
            m_AllyR.Distance(p.Position()) < m_R.Range &&
            std::string(m_AllyR.CharacterName()) == "Blitzcrank" &&
            p.Distance(m_AllyR.Position()) > static_cast<float>(GetSlider("rangeBalista", 300))) {
            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!enemy.IsValid() || !enemy.IsEnemy() || enemy.IsDead()) continue;
                if (SDK::Extensions::IsValidTarget(enemy, FLT_MAX, true) &&
                    enemy.HasBuff("rocketgrab2")) {
                    m_R.Cast();
                }
            }
            if (SDK::Game::Time() - m_grabTime < 1.0f) return;
        }

        SurvivalLogic();

        if (LagFree(0)) SetMana();

        if (m_E.IsReady()) {
            LogicE();
            JungleE();
        }

        if (LagFree(1) && m_Q.IsReady()) LogicQ();
        if (LagFree(2) && LaneClear() && m_Q.IsReady() && GetBool("farmQ")) FarmQ();
        if (LagFree(4) && m_W.IsReady() && None()) LogicW();
        if (m_R.IsReady() && GetBool("autoR")) LogicR();
    }

    // ── C# SurvivalLogic ──
    void SurvivalLogic() {
        const auto p = Player();
        if (m_E.IsReady() && p.HealthPercent() < 50.0f && GetBool("Edead")) {
            const float dmg = OktwCommon::GetIncomingDamage(p);
            if (dmg > 0.0f) {
                if (p.Health() - dmg < p.CountEnemyHeroesInRange(700.0f) * p.Level() * 5.0f)
                    CastE();
                else if (p.Health() - dmg < p.Level() * 5.0f)
                    CastE();
            }
        }

        if (m_R.IsReady() && m_hasAllyR &&
            m_AllyR.IsVisible() &&
            m_AllyR.HealthPercent() < 50.0f &&
            m_AllyR.Distance(p.Position()) < m_R.Range) {
            const float dmg = OktwCommon::GetIncomingDamage(m_AllyR);
            if (dmg > 0.0f) {
                if (m_AllyR.Health() - dmg < m_AllyR.CountEnemyHeroesInRange(700.0f) * m_AllyR.Level() * 10.0f)
                    m_R.Cast();
                else if (m_AllyR.Health() - dmg < m_AllyR.Level() * 10.0f)
                    m_R.Cast();
            }
        }
    }

    // ── C# LogicQ ──
    void LogicQ() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_Q.Range, DamageType::Physical) : AIHeroClient();
        if (!t.IsValid()) return;

        const auto p = Player();
        const auto pout = m_Q.GetPrediction(t);
        const auto col = pout.CollisionObjects;

        bool cast = true;
        for (const auto& colobj : col) {
            if (m_Q.GetDamage(colobj) < colobj.Health()) cast = false;
        }

        const float qDmg = OktwCommon::GetKsDamage(t, m_Q) + p.GetAutoAttackDamage(t, false);
        const float eDmg = GetEdmg(t);

        if (qDmg > t.Health() && eDmg < t.Health() && p.Mana() > m_QMANA + m_EMANA) {
            CastQ(cast, t);
        } else if ((qDmg * 1.1f) + eDmg > t.Health() && eDmg < t.Health() &&
                   p.Mana() > m_QMANA + m_EMANA &&
                   SDK::Core::Utils::AutoAttack::InAutoAttackRange(t)) {
            CastQ(cast, t);
        } else if (Combo() && p.Mana() > m_RMANA + m_QMANA + m_EMANA) {
            const int qMode = GetList("qMode", 1);
            if (qMode == 0) CastQ(cast, t);
            else if (!SDK::Core::Utils::AutoAttack::InAutoAttackRange(t) || CountMeleeInRange(400.0f) > 0)
                CastQ(cast, t);
        } else if (Harass() && !SDK::Core::Utils::AutoAttack::InAutoAttackRange(t) &&
                   GetBool((std::string("Harass") + t.CharacterName()).c_str()) &&
                   !p.IsUnderEnemyTurret() &&
                   p.ManaPercent() > static_cast<float>(GetSlider("qMana", 50))) {
            CastQ(cast, t);
        }

        if ((Combo() || Harass()) && p.Mana() > m_RMANA + m_QMANA + m_EMANA) {
            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                if (SDK::Extensions::IsValidTarget(enemy, m_Q.Range, true) &&
                    !OktwCommon::CanMove(enemy)) {
                    CastQ(cast, t);
                }
            }
        }
    }

    // ── C# FarmQ ──
    void FarmQ() {
        auto minions = OktwCommon::GetMinions(Player().ServerPosition(), m_Q.Range);
        int countMinion = 0;
        SDK::AIMinionClient bestMinion{};
        bool hasBest = false;

        for (const auto& minion : minions) {
            if (minion.HealthPercent() >= 95.0f) continue;
            if (!SDK::Extensions::IsValidTarget(minion, m_Q.Range, true)) continue;
            if (m_Q.GetDamage(minion) <= minion.Health()) continue;

            const auto pout = m_Q.GetPrediction(minion);
            const auto col  = pout.CollisionObjects;
            if (col.empty()) continue;

            for (const auto& colobj : col) {
                if (m_Q.GetDamage(colobj) > colobj.Health()) {
                    ++countMinion;
                    bestMinion = minion;
                    hasBest = true;
                } else {
                    countMinion = 0;
                    hasBest = false;
                    continue;
                }
            }
            countMinion = countMinion / 3;
            countMinion += 1;
        }
        if (hasBest && countMinion >= GetSlider("farmQcount", 2))
            m_Q1.Cast(bestMinion);
    }

    // ── C# LogicE ──
    void LogicE() {
        const int countE = GetSlider("countE", 10);
        const bool eBigMinion = GetBool("minionE");
        int count = 0;
        int outRange = 0;

        auto minions = OktwCommon::GetMinions(Player().ServerPosition(), m_E.Range - 50.0f);

        for (const auto& minion : minions) {
            if (!SDK::Extensions::IsValidTarget(minion, m_E.Range, true)) continue;
            if (minion.HealthPercent() >= 80.0f) continue;

            const float eDmg = m_E.GetDamage(minion);
            if (minion.Health() < eDmg - 0.0f && eDmg > 0.0f) { // TODO(oktw-port): HPRegenRate() not available in SDK
                if (GetPassiveTime(minion) > 0.5f &&
                    SDK::HealthPrediction::GetPrediction(minion, 300, 0) >
                        Player().GetAutoAttackDamage(minion, false) &&
                    !minion.HasBuff("kindredrnodeathbuff")) {
                    ++count;
                    if (!SDK::Core::Utils::AutoAttack::InAutoAttackRange(minion)) ++outRange;
                    if (eBigMinion) {
                        std::string mn = minion.CharacterName();
                        std::transform(mn.begin(), mn.end(), mn.begin(),
                                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                        if (mn.find("siege") != std::string::npos ||
                            mn.find("super") != std::string::npos) {
                            ++outRange;
                        }
                    }
                }
            }
        }

        const bool near700 = Player().CountEnemyHeroesInRange(700.0f) == 0;

        for (const auto& target : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!target.IsValid() || !target.IsEnemy()) continue;
            if (!SDK::Extensions::IsValidTarget(target, m_E.Range, true)) continue;
            if (!target.HasBuff("kalistaexpungemarker")) continue;
            if (!OktwCommon::ValidUlt(target)) continue;

            const float eDmg = GetEdmg(target);
            if (target.Health() < eDmg) CastE();
            if (0 < eDmg && count > 0 && GetBool("Ekillmin")) CastE();
            if (GetEStacks(target) >= countE &&
                (GetPassiveTime(target) < 1.0f || near700) &&
                Player().Mana() > m_RMANA + m_EMANA) {
                CastE();
            }
        }

        if (Farm() && count > 0 && GetBool("farmE")) {
            if (outRange > 0) CastE();
            if ((count >= GetSlider("farmEcount", 2)) ||
                ((Player().IsUnderAllyTurret() && !Player().IsUnderEnemyTurret()) &&
                 Player().Mana() > m_RMANA + m_QMANA + m_EMANA)) {
                CastE();
            }
        }
    }

    // ── C# LogicW (auto-jungle W to fixed positions) ──
    void LogicW() {
        if (!GetBool("Wdragon")) return;
        // C# also gated on !Orbwalker.GetTarget().IsValidTarget() && !Program.Combo &&
        // Player.CountEnemiesInRange(800)==0 — Program.None already ensures !Combo.
        if (Player().CountEnemyHeroesInRange(800.0f) != 0) return;

        const auto p = Player();

        if (m_wCount > 0) {
            Vector3 baronPos{ 5232.0f, 0.0f, 10788.0f };
            if (p.Distance(baronPos) < 5000.0f) m_W.Cast(baronPos);
        }
        if (m_wCount == 0) {
            Vector3 dragonPos{ 9919.0f, 0.0f, 4475.0f };
            if (p.Distance(dragonPos) < 5000.0f) m_W.Cast(dragonPos);
            else ++m_wCount;
            return;
        }
        if (m_wCount == 1) {
            Vector3 redPos{ 8022.0f, 0.0f, 4156.0f };
            if (p.Distance(redPos) < 5000.0f) m_W.Cast(redPos);
            else ++m_wCount;
            return;
        }
        if (m_wCount == 2) {
            Vector3 bluePos{ 11396.0f, 0.0f, 7076.0f };
            if (p.Distance(bluePos) < 5000.0f) m_W.Cast(bluePos);
            else ++m_wCount;
            return;
        }
        if (m_wCount > 2) m_wCount = 0;
    }

    // ── C# JungleE ──
    void JungleE() {
        if (!GetBool("jungleE")) return;
        auto mobs = OktwCommon::GetMinions(Player().ServerPosition(), m_E.Range, false, true);
        if (mobs.empty()) return;
        const auto& mob = mobs.front();
        float dmg = GetEdmg(mob);
        const std::string name = mob.Name();
        if (name.find("Baron") != std::string::npos && Player().HasBuff("barontarget"))
            dmg *= 0.5f;
        if (name.find("Dragon") != std::string::npos && Player().HasBuff("s5test_dragonslayerbuff"))
            dmg *= (1.0f - (0.07f * static_cast<float>(
                OktwCommon::GetBuffCount(Player(), "s5test_dragonslayerbuff"))));
        if (mob.Health() < dmg) CastE();
    }

    // ── C# LogicR ──
    void LogicR() {
        if (!m_hasAllyR) {
            for (const auto& ally : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!ally.IsValid() || ally.IsDead()) continue;
                if (ally.NetworkId() == Player().NetworkId()) continue;
                if (ally.Team() != Player().Team()) continue;
                if (ally.HasBuff("kalistacoopstrikeally")) {
                    m_AllyR = ally;
                    m_hasAllyR = true;
                    break;
                }
            }
        } else if (m_AllyR.IsVisible() && m_AllyR.Distance(Player().Position()) < m_R.Range) {
            if (m_AllyR.Health() < m_AllyR.CountEnemyHeroesInRange(600.0f) * m_AllyR.Level() * 30.0f) {
                m_R.Cast();
            }
        }
    }

    void OnGameDraw() override {}
};

} } // namespace Plugins::OKTW
