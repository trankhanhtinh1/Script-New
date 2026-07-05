#pragma once
// Port of OKTW_CSharp/Champions/Ezreal.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;
using SDK::UI::MenuKeyBind;

class EzrealPlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Ezreal"; }
    const char* GetInternalId() const override { return "champion.oktw.ezreal"; }
    const char* GetChampionName() const override { return "Ezreal"; }

protected:
    Vector3 m_cursorPosition{};
    bool    m_eSmart = false;
    float   m_overKill = 0.0f;
    float   m_dragonDmg = 0.0f;
    float   m_dragonTime = 0.0f;
    static constexpr int Muramana = 3042;
    static constexpr int Tear     = 3070;
    static constexpr int Manamune = 3004;

    void BuildMenu() override {
        MarkActive();

        m_Q = Spell(SpellSlot::Q, 1170.0f);
        m_W = Spell(SpellSlot::W, 950.0f);
        m_E = Spell(SpellSlot::E, 475.0f);
        m_R = Spell(SpellSlot::R, 3000.0f);

        m_Q.SetSkillshot(0.25f, 60.0f,  2000.0f, true,  SDK::SpellType::Line);
        m_W.SetSkillshot(0.25f, 80.0f,  1600.0f, false, SDK::SpellType::Line);
        m_R.SetSkillshot(1.10f, 160.0f, 2000.0f, false, SDK::SpellType::Line);

        m_drawMenu->Add(new MenuBool("noti",    "Show notification",       false));
        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw only ready spells",  true));
        m_drawMenu->Add(new MenuBool("qRange",  "Q range", false));
        m_drawMenu->Add(new MenuBool("wRange",  "W range", false));
        m_drawMenu->Add(new MenuBool("eRange",  "E range", false));
        m_drawMenu->Add(new MenuBool("rRange",  "R range", false));

        m_wMenu->Add(new MenuBool("autoW",   "Auto W", true));
        m_wMenu->Add(new MenuBool("wPush",   "W ally (push tower)", true));
        m_wMenu->Add(new MenuBool("harassW", "Harass W", true));

        m_eMenu->Add(new MenuKeyBind("smartE",    "SmartCast E key",     'T', SDK::KeyBindType::Press));
        m_eMenu->Add(new MenuKeyBind("smartEW",   "SmartCast E + W key", 'T', SDK::KeyBindType::Press));
        m_eMenu->Add(new MenuBool   ("EKsCombo",  "E ks combo",           true));
        m_eMenu->Add(new MenuBool   ("EAntiMelee","E anti-melee",         true));
        m_eMenu->Add(new MenuBool   ("autoEgrab", "Auto E anti grab",     true));

        m_rMenu->Add(new MenuBool("autoR", "Auto R", true));
        m_rMenu->Add(new MenuBool("Rcc",   "R cc",   true));
        m_rMenu->Add(new MenuSlider("Raoe", "R AOE", 3, 0, 5));

        Menu* jungle = m_rMenu->AddSubMenu(new Menu("RjungleSub", "R Jungle stealer"));
        jungle->Add(new MenuBool("Rjungle", "R Jungle stealer", true));
        jungle->Add(new MenuBool("Rdragon", "Dragon", true));
        jungle->Add(new MenuBool("Rbaron",  "Baron",  true));
        jungle->Add(new MenuBool("Rred",    "Red",    true));
        jungle->Add(new MenuBool("Rblue",   "Blue",   true));
        jungle->Add(new MenuBool("Rally",   "Ally stealer", false));

        m_rMenu->Add(new MenuKeyBind("useR",     "Semi-manual cast R key", 'T', SDK::KeyBindType::Press));
        m_rMenu->Add(new MenuBool   ("Rturrent", "Don't R under turret",   true));
        m_rMenu->Add(new MenuSlider ("MaxRangeR","Max R range", 3000, 0, 5000));
        m_rMenu->Add(new MenuSlider ("MinRangeR","Min R range",  900, 0, 5000));

        m_menu->Add(new MenuSlider("HarassMana", "Harass Mana", 30, 0, 100));

        m_farmMenu->Add(new MenuBool("farmQ", "LaneClear Q",       true));
        m_farmMenu->Add(new MenuBool("FQ",    "Farm Q out range",  true));
        m_farmMenu->Add(new MenuBool("LCP",   "FAST LaneClear",    true));

        m_menu->Add(new MenuBool("apEz",  "AP Ezreal",             false));
        m_menu->Add(new MenuBool("stack", "Stack Tear if full mana", false));
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
        if (LagFree(0)) SetMana();

        if (m_R.IsReady() && GetBool("Rjungle")) {
            KsJungle();
        } else {
            m_dragonTime = 0.0f;
        }

        if (m_E.IsReady()) {
            if (LagFree(0)) LogicE();

            const auto player = Player();
            if (GetKey("smartE")) m_eSmart = true;
            if (GetKey("smartEW") && m_W.IsReady()) {
                m_cursorPosition = SDK::Game::CursorPos();
                m_W.Cast(m_cursorPosition);
            }
            if (m_eSmart) {
                const Vector3 dir = (SDK::Game::CursorPos() - player.Position());
                const float len = dir.Length();
                Vector3 target = player.Position();
                if (len > 0.001f) target = player.Position() + dir * (m_E.Range / len);
                if (OktwCommon::CountEnemiesInRange(target, 500.0f) < 4)
                    m_E.Cast(target);
            }

            if (m_cursorPosition.Length() > 0.001f) {
                const Vector3 dir = (m_cursorPosition - player.Position());
                const float len = dir.Length();
                Vector3 target = player.Position();
                if (len > 0.001f) target = player.Position() + dir * (m_E.Range / len);
                m_E.Cast(target);
            }
        } else {
            m_cursorPosition = Vector3{};
            m_eSmart = false;
        }

        if (m_Q.IsReady()) LogicQ();

        if (LagFree(3) && m_W.IsReady() && GetBool("autoW")) LogicW();

        if (m_R.IsReady()) {
            if (GetKey("useR")) {
                auto* ts = SDK::TargetSelector::Instance();
                auto t = ts ? ts->GetTarget(m_R.Range, DamageType::Magical) : AIHeroClient();
                if (t.IsValid()) m_R.Cast(t);
            }
            if (LagFree(4)) LogicR();
        }
    }

    void LogicQ() {
        const auto player = Player();
        if (LagFree(1)) {
            const bool cc     = !None() && player.Mana() > m_RMANA + m_QMANA + m_EMANA;
            const bool harass = Harass() && player.ManaPercent() > (float)GetSlider("HarassMana", 30) &&
                                OktwCommon::CanHarras();

            if (Combo() && player.Mana() > m_RMANA + m_QMANA) {
                auto* ts = SDK::TargetSelector::Instance();
                auto t = ts ? ts->GetTarget(m_Q.Range, DamageType::Physical) : AIHeroClient();
                if (t.IsValid()) CastSpell(m_Q, t);
            }

            for (const auto& t : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!t.IsValid() || !t.IsEnemy()) continue;
                if (!SDK::Extensions::IsValidTarget(t, m_Q.Range, true)) continue;

                const float qDmg = OktwCommon::GetKsDamage(t, m_Q);
                const float wDmg = m_W.GetDamage(t);
                if (qDmg + wDmg > t.Health()) {
                    CastSpell(m_Q, t);
                    m_overKill = SDK::Game::Time();
                    return;
                }

                if (cc && !OktwCommon::CanMove(t)) m_Q.Cast(t);

                if (harass && GetBool((std::string("Harass") + t.CharacterName()).c_str()))
                    CastSpell(m_Q, t);
            }
        } else if (LagFree(2)) {
            if (player.Mana() > m_QMANA && FarmSpells()) {
                FarmQ();
            } else if (GetBool("stack") && !player.HasBuff("Recall") &&
                       player.Mana() > player.MaxMana() * 0.95f && None() &&
                       (player.HasItem(Tear) || player.HasItem(Manamune))) {
                const Vector3 dir = (SDK::Game::CursorPos() - player.Position());
                const float len = dir.Length();
                Vector3 target = player.Position();
                if (len > 0.001f) target = player.Position() + dir * (500.0f / len);
                m_Q.Cast(target);
            }
        }
    }

    void LogicW() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_W.Range, DamageType::Magical) : AIHeroClient();
        const auto player = Player();
        if (t.IsValid()) {
            if (Combo() && player.Mana() > m_RMANA + m_WMANA + m_EMANA) {
                CastSpell(m_W, t);
            } else if (Harass() && GetBool("harassW") &&
                       GetBool((std::string("Harass") + t.CharacterName()).c_str()) &&
                       (player.Mana() > player.MaxMana() * 0.8f || GetBool("apEz")) &&
                       player.ManaPercent() > (float)GetSlider("HarassMana", 30) &&
                       OktwCommon::CanHarras()) {
                CastSpell(m_W, t);
            } else {
                const float qDmg = m_Q.GetDamage(t);
                const float wDmg = OktwCommon::GetKsDamage(t, m_W);
                if (wDmg > t.Health()) {
                    CastSpell(m_W, t);
                    m_overKill = SDK::Game::Time();
                } else if (wDmg + qDmg > t.Health() && m_Q.IsReady()) {
                    CastSpell(m_W, t);
                }
            }

            if (!None() && player.Mana() > m_RMANA + m_WMANA + m_EMANA) {
                for (const auto& enemy : SDK::ObjectManager::Get<AIHeroClient>()) {
                    if (!enemy.IsValid() || !enemy.IsEnemy()) continue;
                    if (SDK::Extensions::IsValidTarget(enemy, m_W.Range, true) &&
                        !OktwCommon::CanMove(enemy)) {
                        m_W.Cast(enemy);
                    }
                }
            }
        }
    }

    void LogicE() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(1300.0f, DamageType::Physical) : AIHeroClient();
        const auto player = Player();

        if (GetBool("EAntiMelee")) {
            for (const auto& target : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!target.IsValid() || !target.IsEnemy()) continue;
                if (!SDK::Extensions::IsValidTarget(target, 1000.0f, true)) continue;
                if (!target.IsMelee()) continue;
                const auto pred = m_Q.GetPrediction(target, false);
                if (player.Position().Distance(pred.GetCastPosition()) < 250.0f) {
                    // TODO(oktw-port): dash target selection - simplified: cast toward cursor
                    const Vector3 dir = (SDK::Game::CursorPos() - player.Position());
                    const float len = dir.Length();
                    Vector3 dashPos = player.Position();
                    if (len > 0.001f) dashPos = player.Position() + dir * (m_E.Range / len);
                    m_E.Cast(dashPos);
                    break;
                }
            }
        }

        if (t.IsValid() && Combo() && GetBool("EKsCombo") && player.HealthPercent() > 40.0f &&
            t.Position().Distance(SDK::Game::CursorPos()) + 300.0f < t.Position().Distance(player.Position()) &&
            (SDK::Game::Time() - m_overKill > 0.3f)) {
            const Vector3 dir = (SDK::Game::CursorPos() - player.Position());
            const float len = dir.Length();
            Vector3 dashPosition = player.Position();
            if (len > 0.001f) dashPosition = player.Position() + dir * (m_E.Range / len);

            if (OktwCommon::CountEnemiesInRange(dashPosition, 900.0f) < 3) {
                float dmgCombo = 0.0f;

                if (SDK::Extensions::IsValidTarget(t, 950.0f, true)) {
                    dmgCombo = player.GetAutoAttackDamage(t, false) + m_E.GetDamage(t);
                }

                if (m_Q.IsReady() && player.Mana() > m_QMANA + m_EMANA)
                    dmgCombo = m_Q.GetDamage(t);

                if (m_W.IsReady() && player.Mana() > m_QMANA + m_EMANA + m_WMANA)
                    dmgCombo += m_W.GetDamage(t);

                if (dmgCombo > t.Health() && OktwCommon::ValidUlt(t)) {
                    m_E.Cast(dashPosition);
                    m_overKill = SDK::Game::Time();
                }
            }
        }
    }

    void LogicR() {
        const auto player = Player();
        // UnderTurret not exposed - TODO(oktw-port): wire when SDK exposes it
        // if (GetBool("Rturrent") && player.UnderTurret()) return;

        if (GetBool("autoR") && OktwCommon::CountEnemiesInRange(player.Position(), 800.0f) == 0 &&
            SDK::Game::Time() - m_overKill > 0.6f) {
            const float maxR = (float)GetSlider("MaxRangeR", 3000);
            for (const auto& target : SDK::ObjectManager::Get<AIHeroClient>()) {
                if (!target.IsValid() || !target.IsEnemy()) continue;
                if (!SDK::Extensions::IsValidTarget(target, maxR, true)) continue;
                if (!OktwCommon::ValidUlt(target)) continue;

                const float predictedHealth = target.Health() - OktwCommon::GetIncomingDamage(target);

                if (GetBool("Rcc") && SDK::Extensions::IsValidTarget(target, m_Q.Range + m_E.Range, true) &&
                    target.Health() < player.MaxHealth() && !OktwCommon::CanMove(target)) {
                    m_R.Cast(target);
                }

                const float rDmg = m_R.GetDamage(target);
                if (rDmg > predictedHealth &&
                    OktwCommon::CountAlliesInRange(target.Position(), 500.0f) == 0 &&
                    player.Position().Distance(target.Position()) > (float)GetSlider("MinRangeR", 900)) {
                    CastSpell(m_R, target);
                }

                if (Combo() && OktwCommon::CountEnemiesInRange(player.Position(), 1200.0f) == 0) {
                    const int aoe = GetSlider("Raoe", 3);
                    auto p = m_R.GetPrediction(target, true);
                    if (p.AoeTargetsHitCount >= aoe)
                        m_R.Cast(p.GetCastPosition());
                }
            }
        }
    }

    void FarmQ() {
        const auto player = Player();
        if (LaneClear()) {
            auto mobs = OktwCommon::GetMinions(player.ServerPosition(), 800.0f, false, true);
            if (!mobs.empty()) {
                const auto& mob = mobs.front();
                m_Q.Cast(mob.Position());
            }
        }

        auto minions = OktwCommon::GetMinions(player.ServerPosition(), m_Q.Range);

        if (GetBool("FQ")) {
            for (const auto& mn : minions) {
                if (mn.Health() > 0 && mn.Health() < m_Q.GetDamage(mn)) {
                    if (m_Q.Cast(mn) == SDK::CastStates::SuccessfullyCasted) return;
                }
            }
        }

        if (GetBool("farmQ") && FarmSpells()) {
            const bool LCP = GetBool("LCP");
            const float passiveT = OktwCommon::GetPassiveTime(player, "ezrealrisingspellforce");
            const bool PT = (SDK::Game::Time() - passiveT > -1.5f) || !m_E.IsReady();

            for (const auto& mn : minions) {
                const float qDmg = m_Q.GetDamage(mn);
                if (mn.Health() < qDmg) {
                    if (m_Q.Cast(mn) == SDK::CastStates::SuccessfullyCasted) return;
                } else if (PT || LCP) {
                    if (mn.HealthPercent() > 80.0f) {
                        if (m_Q.Cast(mn) == SDK::CastStates::SuccessfullyCasted) return;
                    }
                }
            }
        }
    }

    void KsJungle() {
        const auto player = Player();
        auto mobs = OktwCommon::GetMinions(player.ServerPosition(), FLT_MAX, false, true);
        for (const auto& mob : mobs) {
            if (mob.Health() >= mob.MaxHealth()) continue;

            const std::string name = mob.CharacterName();
            std::string lower;
            lower.reserve(name.size());
            for (char c : name) lower.push_back((char)::tolower((unsigned char)c));

            const bool isDragon = lower.find("dragon") != std::string::npos;
            const bool isBaron  = name == "SRU_Baron";
            const bool isRed    = name == "SRU_Red";
            const bool isBlue   = name == "SRU_Blue";

            const bool wanted = (isDragon && GetBool("Rdragon")) ||
                                (isBaron  && GetBool("Rbaron"))  ||
                                (isRed    && GetBool("Rred"))    ||
                                (isBlue   && GetBool("Rblue"));

            if (!wanted) continue;
            if (OktwCommon::CountAlliesInRange(mob.Position(), 1000.0f) > 0 && !GetBool("Rally")) continue;
            if (mob.Position().Distance(player.Position()) <= 1000.0f) continue;

            if (m_dragonDmg == 0.0f) m_dragonDmg = mob.Health();

            if (SDK::Game::Time() - m_dragonTime > 3.0f) {
                if (m_dragonDmg - mob.Health() > 0.0f) m_dragonDmg = mob.Health();
                m_dragonTime = SDK::Game::Time();
            } else {
                const float dt = std::abs(m_dragonTime - SDK::Game::Time());
                const float dmgSec = (m_dragonDmg - mob.Health()) * (dt / 3.0f);
                if (m_dragonDmg - mob.Health() > 0.0f) {
                    const float distance = player.ServerPosition().Distance(mob.Position());
                    const float timeTravel = distance / m_R.Speed + m_R.Delay;
                    const float timeR = (mob.Health() - m_R.GetDamage(mob)) / (dmgSec / 3.0f);
                    if (timeTravel > timeR) m_R.Cast(mob.Position());
                } else {
                    m_dragonDmg = mob.Health();
                }
            }
        }
    }

    // TODO(oktw-port): wire when SDK event exposed
    void OnBuffAdd(const AIHeroClient& /*sender*/, const std::string& /*buffName*/) {
        // ThreshQ / rocketgrab2 anti-grab E-dash
        // TODO(oktw-port): wire when OnBuffGain event exposed
    }

    // TODO(oktw-port): wire when SDK event exposed
    void OnAfterAttack(const AIHeroClient& /*target*/) {
        // W-push ally when attacking a tower
        // TODO(oktw-port): wire when Orbwalking.AfterAttack exposed
    }

    void OnGameDraw() override {}
};

} } // namespace Plugins::OKTW
