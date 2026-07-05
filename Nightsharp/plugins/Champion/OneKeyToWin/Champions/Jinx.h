#pragma once
// Port of OKTW_CSharp/Champions/Jinx.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;
using SDK::UI::MenuKeyBind;

class JinxPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Jinx"; }
    const char* GetInternalId() const override { return "champion.oktw.jinx"; }
    const char* GetChampionName() const override { return "Jinx"; }

protected:
    // ── Runtime state (mirrors C# publics) ──
    double m_lag        = 0.0;
    double m_WCastTime  = 0.0;
    double m_QCastTime  = 0.0;
    double m_DragonTime = 0.0;
    double m_grabTime   = 0.0;
    float  m_DragonDmg  = 0.0f;

    // ── Helper: FishBone (rocket) form active ──
    bool FishBoneActive() const { return Player().HasBuff("JinxQ"); }

    // ── Player attack range with bonusRange (C# private float bonusRange) ──
    // NB: C# reads Spellbook.GetSpell(SpellSlot.Q).Level; approximated with
    // m_Q.Level() — TODO(oktw-port) if the SDK exposes it differently.
    float BonusRange() const {
        const auto p = Player();
        return 670.0f + p.BoundingRadius() + 25.0f * static_cast<float>(m_Q.Level());
    }

    float GetRealPowPowRange(const SDK::GameObject& target) const {
        return 620.0f + Player().BoundingRadius() + target.BoundingRadius();
    }

    float GetRealDistance(const AIBaseClient& target) const {
        const auto p = Player();
        const Vector3 predPos = SDK::Prediction::Movement::GetPrediction(target, 0.05f).GetCastPosition();
        return p.ServerPosition().Distance(predPos) + p.BoundingRadius() + target.BoundingRadius();
    }

    static bool ShouldUseE(const std::string& spellName) {
        static const std::vector<std::string> triggers = {
            "ThreshQ","KatarinaR","AlZaharNetherGrasp","GalioIdolOfDurand",
            "LuxMaliceCannon","MissFortuneBulletTime","RocketGrabMissile",
            "CaitlynPiltoverPeacemaker","EzrealTrueshotBarrage","InfiniteDuress","VelkozR"
        };
        for (const auto& s : triggers) if (s == spellName) return true;
        return false;
    }

    void BuildMenu() override {
        MarkActive();

        m_Q = Spell(SpellSlot::Q);
        m_W = Spell(SpellSlot::W, 1500.0f);
        m_E = Spell(SpellSlot::E, 920.0f);
        m_R = Spell(SpellSlot::R, 3000.0f);

        m_W.SetSkillshot(0.6f, 60.0f, 3300.0f, true,  SDK::SpellType::Line);
        m_E.SetSkillshot(1.2f, 100.0f, 1750.0f, false, SDK::SpellType::Circle);
        m_R.SetSkillshot(0.7f, 140.0f, 1500.0f, false, SDK::SpellType::Line);

        // Draw
        m_drawMenu->Add(new MenuBool("noti",     "Show notification", false));
        m_drawMenu->Add(new MenuBool("semi",     "Semi-manual R target", false));
        m_drawMenu->Add(new MenuBool("qRange",   "Q range", false));
        m_drawMenu->Add(new MenuBool("wRange",   "W range", false));
        m_drawMenu->Add(new MenuBool("eRange",   "E range", false));
        m_drawMenu->Add(new MenuBool("rRange",   "R range", false));
        m_drawMenu->Add(new MenuBool("onlyRdy",  "Draw only ready spells", true));

        // Q
        m_qMenu->Add(new MenuBool("autoQ",   "Auto Q", true));
        m_qMenu->Add(new MenuBool("Qharras", "Harass Q", true));

        // W
        m_wMenu->Add(new MenuBool("autoW",   "Auto W", true));
        m_wMenu->Add(new MenuBool("Wharras", "Harass W", true));

        // E
        m_eMenu->Add(new MenuBool("autoE",  "Auto E on CC", true));
        m_eMenu->Add(new MenuBool("comboE", "Auto E in Combo BETA", true));
        m_eMenu->Add(new MenuBool("AGC",    "AntiGapcloserE", true));
        m_eMenu->Add(new MenuBool("opsE",   "OnProcessSpellCastE", true));
        m_eMenu->Add(new MenuBool("telE",   "Auto E teleport", true));

        // R
        m_rMenu->Add(new MenuBool("autoR", "Auto R", true));
        Menu* rJungle = m_rMenu->AddSubMenu(new Menu("RJungleSub", "R Jungle stealer"));
        rJungle->Add(new MenuBool("Rjungle", "R Jungle stealer", true));
        rJungle->Add(new MenuBool("Rdragon", "Dragon", true));
        rJungle->Add(new MenuBool("Rbaron",  "Baron", true));
        m_rMenu->Add(new MenuSlider("hitchanceR", "Hit Chance R", 2, 0, 3));
        m_rMenu->Add(new MenuKeyBind("useR", "OneKeyToCast R", 'T', SDK::KeyBindType::Press));
        m_rMenu->Add(new MenuBool("Rturrent", "Don't R under turret", true));

        // Farm
        m_farmMenu->Add(new MenuBool("farmQout", "Q farm out range AA", true));
        m_farmMenu->Add(new MenuBool("farmQ",    "Q LaneClear Q", true));

        // TODO(oktw-port): AntiGapcloser::OnEnemyGapcloser handler (E on dash).
        // TODO(oktw-port): Obj_AI_Base::OnProcessSpellCast for:
        //   - self JinxWMissile → capture WCastTime
        //   - enemy dangerous-spell → cast E on server-position
        //   - ally RocketGrab in range → grabTime
        // TODO(oktw-port): SebbyLib.Orbwalking.BeforeAttack — pre-attack Q swap logic.
    }

    void SetMana() override {
        if ((Shared().manaDisable && Shared().manaDisable->Value && Combo()) ||
            Player().HealthPercent() < 20.0f) {
            m_QMANA = m_WMANA = m_EMANA = m_RMANA = 0.0f;
            return;
        }
        m_QMANA = 10.0f;
        m_WMANA = m_W.Instance().ManaCost();
        m_EMANA = m_E.Instance().ManaCost();
        if (!m_R.IsReady())
            m_RMANA = m_WMANA - 0.0f * m_W.Instance().Cooldown(); // TODO(oktw-port): PARRegenRate unavailable
        else
            m_RMANA = m_R.Instance().ManaCost();
    }

    void OnGameUpdate() override {
        if (m_R.IsReady()) {
            if (GetKey("useR")) {
                auto* ts = SDK::TargetSelector::Instance();
                auto t = ts ? ts->GetTarget(m_R.Range, DamageType::Physical) : AIHeroClient();
                if (t.IsValid()) m_R.Cast(t);
            }
            if (GetBool("Rjungle")) {
                KsJungle();
            }
        }

        if (LagFree(0)) SetMana();

        if (m_E.IsReady()) LogicE();

        if (LagFree(2) && m_Q.IsReady() && GetBool("autoQ"))
            LogicQ();

        if (LagFree(3) && m_W.IsReady() && !Player().Spellbook().IsWindingUp() && GetBool("autoW"))
            LogicW();

        if (LagFree(4) && m_R.IsReady())
            LogicR();
    }

    // ── LogicQ ────────────────────────────────────────────────────────────────
    void LogicQ() {
        const auto p = Player();
        auto* ts = SDK::TargetSelector::Instance();

        if (Farm() && !FishBoneActive() && !p.Spellbook().IsWindingUp() &&
            GetBool("farmQout") &&
            p.Mana() > m_RMANA + m_WMANA + m_EMANA + 10.0f) {
            auto minions = OktwCommon::GetMinions(p.Position(), BonusRange() + 30.0f);
            for (const auto& mn : minions) {
                if (GetRealPowPowRange(mn) < GetRealDistance(mn) &&
                    BonusRange() < GetRealDistance(mn)) {
                    const float hpPred = SDK::HealthPrediction::GetPrediction(mn, 400, 70);
                    if (hpPred < p.GetAutoAttackDamage(mn, false) * 1.1f && hpPred > 5.0f) {
                        // TODO(oktw-port): Orbwalker.ForceTarget(minion) — no direct binding.
                        m_Q.Cast();
                        return;
                    }
                }
            }
        }

        auto t = ts ? ts->GetTarget(BonusRange() + 60.0f, DamageType::Physical) : AIHeroClient();
        if (t.IsValid()) {
            if (!FishBoneActive() &&
                (OktwCommon::CountEnemiesInRange(t.Position(), 250.0f) > 2)) {
                const float distance = GetRealDistance(t);
                (void)distance;
                if (Combo() && (p.Mana() > m_RMANA + m_WMANA + 10.0f ||
                                p.GetAutoAttackDamage(t, false) * 3.0f > t.Health()))
                    m_Q.Cast();
                else if (Harass() && !p.Spellbook().IsWindingUp() &&
                         GetBool("Qharras") && !p.IsUnderEnemyTurret() &&
                         p.Mana() > m_RMANA + m_WMANA + m_EMANA + 20.0f &&
                         distance < BonusRange() + t.BoundingRadius() + p.BoundingRadius())
                    m_Q.Cast();
            }
        } else if (!FishBoneActive() && Combo() &&
                   p.Mana() > m_RMANA + m_WMANA + 20.0f &&
                   p.CountEnemyHeroesInRange(2000.0f) > 0) {
            m_Q.Cast();
        } else if (FishBoneActive() && Combo() &&
                   p.Mana() < m_RMANA + m_WMANA + 20.0f) {
            m_Q.Cast();
        } else if (FishBoneActive() && Combo() &&
                   p.CountEnemyHeroesInRange(2000.0f) == 0) {
            m_Q.Cast();
        } else if (FishBoneActive() && Farm()) {
            m_Q.Cast();
        }
    }

    // ── LogicW ────────────────────────────────────────────────────────────────
    void LogicW() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_W.Range, DamageType::Physical) : AIHeroClient();
        const auto p = Player();
        if (t.IsValid()) {
            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                if (!SDK::Extensions::IsValidTarget(enemy, m_W.Range, true)) continue;
                if (enemy.Position().Distance(p.Position()) <= BonusRange()) continue;

                float comboDmg = OktwCommon::GetKsDamage(enemy, m_W);
                if (m_R.IsReady() && p.Mana() > m_RMANA + m_WMANA + 20.0f)
                    comboDmg += m_R.GetDamage(enemy);
                if (comboDmg > enemy.Health() && OktwCommon::ValidUlt(enemy)) {
                    CastSpell(m_W, enemy);
                    return;
                }
            }

            if (p.CountEnemyHeroesInRange(BonusRange()) == 0) {
                if (Combo() && p.Mana() > m_RMANA + m_WMANA + 10.0f) {
                    std::vector<AIHeroClient> pool;
                    for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                        if (enemy.IsValid() && enemy.IsEnemy() &&
                            SDK::Extensions::IsValidTarget(enemy, m_W.Range, true) &&
                            GetRealDistance(enemy) > BonusRange())
                            pool.push_back(enemy);
                    }
                    std::sort(pool.begin(), pool.end(),
                              [](const AIHeroClient& a, const AIHeroClient& b){ return a.Health() < b.Health(); });
                    for (const auto& enemy : pool)
                        CastSpell(m_W, enemy);
                } else if (Harass() && p.Mana() > m_RMANA + m_EMANA + m_WMANA + m_WMANA + 40.0f &&
                           OktwCommon::CanHarras() && GetBool("Wharras")) {
                    for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                        if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                        if (SDK::Extensions::IsValidTarget(enemy, m_W.Range, true) &&
                            GetBool((std::string("Harass") + enemy.CharacterName()).c_str()))
                            CastSpell(m_W, enemy);
                    }
                }
            }
            if (!None() && p.Mana() > m_RMANA + m_WMANA &&
                p.CountEnemyHeroesInRange(GetRealPowPowRange(t)) == 0) {
                for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                    if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                    if (SDK::Extensions::IsValidTarget(enemy, m_W.Range, true) &&
                        !OktwCommon::CanMove(enemy))
                        m_W.Cast(enemy.Position());
                }
            }
        }
    }

    // ── LogicE ────────────────────────────────────────────────────────────────
    void LogicE() {
        const auto p = Player();
        if (p.Mana() > m_RMANA + m_EMANA && GetBool("autoE") &&
            SDK::Game::Time() - m_grabTime > 1.0) {
            for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                if (SDK::Extensions::IsValidTarget(enemy, m_E.Range + 50.0f, true) &&
                    !OktwCommon::CanMove(enemy)) {
                    m_E.Cast(enemy);
                    return;
                }
            }
            if (!LagFree(1)) return;

            if (GetBool("telE")) {
                // TODO(oktw-port): OktwCommon.GetTrapPos(E.Range) — teleport trap heuristic.
            }

            if (Combo() && p.IsMoving() && GetBool("comboE") &&
                p.Mana() > m_RMANA + m_EMANA + m_WMANA) {
                auto* ts = SDK::TargetSelector::Instance();
                auto t = ts ? ts->GetTarget(m_E.Range, DamageType::Physical) : AIHeroClient();
                if (t.IsValid() && SDK::Extensions::IsValidTarget(t, m_E.Range, true)) {
                    const Vector3 castPos = m_E.GetPrediction(t).GetCastPosition();
                    if (castPos.Distance(t.Position()) > 200.0f) {
                        // Approximation of E.CastIfWillHit(t, 2)
                        const auto pout = m_E.GetPrediction(t, true);
                        if (pout.AoeTargetsHitCount >= 2)
                            m_E.Cast(pout.GetCastPosition());

                        // TODO(oktw-port): BuffType::Slow unavailable in SDK
                        if (false)
                            CastSpell(m_E, t);

                        if (OktwCommon::IsMovingInSameDirection(p, t))
                            CastSpell(m_E, t);
                    }
                }
            }
        }
    }

    // ── LogicR ────────────────────────────────────────────────────────────────
    void LogicR() {
        const auto p = Player();
        if (p.IsUnderEnemyTurret() && GetBool("Rturrent")) return;
        if (SDK::Game::Time() - m_WCastTime > 0.9 && GetBool("autoR")) {
            for (const auto& target : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!target.IsValid() || !target.IsEnemy()) continue;
                if (!SDK::Extensions::IsValidTarget(target, m_R.Range, true)) continue;
                if (!OktwCommon::ValidUlt(target)) continue;

                const float predictedHealth = target.Health() - OktwCommon::GetIncomingDamage(target);
                const float rDmg = m_R.GetDamage(target);

                if (rDmg > predictedHealth &&
                    !OktwCommon::IsSpellHeroCollision(target, m_R) &&
                    GetRealDistance(target) > BonusRange() + 200.0f) {
                    if (GetRealDistance(target) > BonusRange() + 300.0f + target.BoundingRadius() &&
                        OktwCommon::CountAlliesInRange(target.Position(), 500.0f) == 0 &&
                        p.CountEnemyHeroesInRange(400.0f) == 0) {
                        CastR(target);
                    } else if (OktwCommon::CountEnemiesInRange(target.Position(), 200.0f) > 2) {
                        m_R.Cast(target);
                    }
                }
            }
        }
    }

    void CastR(const AIHeroClient& target) {
        const int idx = GetSlider("hitchanceR", 2);
        if (idx == 0) {
            m_R.Cast(m_R.GetPrediction(target).GetCastPosition());
        } else if (idx == 1) {
            m_R.Cast(target);
        } else if (idx == 2) {
            CastSpell(m_R, target);
        } else if (idx == 3) {
            const auto path = target.Path();
            if (!path.empty()) {
                const Vector3 last = path.back();
                if ((Player().Position().Distance(last) - Player().Position().Distance(target.Position())) > 400.0f)
                    CastSpell(m_R, target);
            }
        }
    }

    // ── R jungle steal (Baron/Dragon) ────────────────────────────────────────
    void KsJungle() {
        const auto p = Player();
        auto mobs = OktwCommon::GetMinions(p.ServerPosition(), FLT_MAX, false, true);
        for (const auto& mob : mobs) {
            const std::string skin = mob.CharacterName();
            std::string lower;
            lower.reserve(skin.size());
            for (char c : skin) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            const bool isDragon = lower.find("dragon") != std::string::npos;
            const bool isBaron  = (skin == "SRU_Baron");

            const bool wantDragon = isDragon && GetBool("Rdragon");
            const bool wantBaron  = isBaron  && GetBool("Rbaron");
            if (!wantDragon && !wantBaron) continue;
            if (!(mob.Health() < mob.MaxHealth())) continue;
            if (OktwCommon::CountAlliesInRange(mob.Position(), 1000.0f) != 0) continue;
            if (mob.Position().Distance(p.Position()) <= 1000.0f) continue;

            if (m_DragonDmg == 0.0f) m_DragonDmg = mob.Health();

            if (SDK::Game::Time() - m_DragonTime > 4.0) {
                if (m_DragonDmg - mob.Health() > 0.0f)
                    m_DragonDmg = mob.Health();
                m_DragonTime = SDK::Game::Time();
            } else {
                const float dmgSec = (m_DragonDmg - mob.Health()) *
                                     (std::abs(static_cast<float>(m_DragonTime - SDK::Game::Time())) / 4.0f);
                if (m_DragonDmg - mob.Health() > 0.0f) {
                    const float timeTravel = GetUltTravelTime(p, m_R.Speed, 0.7f, mob.Position());
                    const float rDmgRaw = (250.0f + 100.0f * static_cast<float>(m_R.Level())) + p.BonusAttackDamage() + 300.0f;
                    const float rDmg = p.CalculatePhysicalDamage(mob, rDmgRaw);
                    const float timeR = (mob.Health() - rDmg) / (dmgSec / 4.0f);
                    if (timeTravel > timeR)
                        m_R.Cast(mob.Position());
                } else {
                    m_DragonDmg = mob.Health();
                }
            }
        }
    }

    float GetUltTravelTime(const AIHeroClient& source, float speed, float delay, const Vector3& targetpos) const {
        const float distance = source.ServerPosition().Distance(targetpos);
        float missilespeed = speed;
        if (source.CharacterName() == "Jinx" && distance > 1350.0f) {
            constexpr float accelerationRate = 0.3f;
            float acceldifference = distance - 1350.0f;
            if (acceldifference > 150.0f) acceldifference = 150.0f;
            const float difference = distance - 1500.0f;
            missilespeed = (1350.0f * speed +
                            acceldifference * (speed + accelerationRate * acceldifference) +
                            difference * 2200.0f) / distance;
        }
        return distance / missilespeed + delay;
    }

    void OnGameDraw() override {
        // Simplified: draw range circles via SDK utilities if desired.
    }
};

} } // namespace Plugins::OKTW
