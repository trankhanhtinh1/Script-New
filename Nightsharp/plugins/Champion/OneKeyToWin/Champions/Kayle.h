#pragma once
// Port of OKTW_CSharp/Champions/Kayle.cs
#include "../OKTWBase.h"

namespace Plugins { namespace OKTW {

using SDK::UI::Menu;
using SDK::UI::MenuBool;
using SDK::UI::MenuList;
using SDK::UI::MenuSlider;

class KaylePlugin final : public OKTWBase {
public:
    const char* GetName() const override { return "OKTW Kayle"; }
    const char* GetInternalId() const override { return "champion.oktw.kayle"; }
    const char* GetChampionName() const override { return "Kayle"; }

protected:
    void BuildMenu() override {
        MarkActive();

        m_Q = Spell(SpellSlot::Q, 670.0f);
        m_W = Spell(SpellSlot::W, 900.0f);
        m_E = Spell(SpellSlot::E, 660.0f);
        m_R = Spell(SpellSlot::R, 900.0f);

        m_drawMenu->Add(new MenuBool("noti",    "Show notification & line", true));
        m_drawMenu->Add(new MenuBool("onlyRdy", "Draw only ready spells",   true));
        m_drawMenu->Add(new MenuBool("qRange",  "Q range", false));
        m_drawMenu->Add(new MenuBool("wRange",  "W range", false));
        m_drawMenu->Add(new MenuBool("eRange",  "E range", false));
        m_drawMenu->Add(new MenuBool("rRange",  "R range", false));

        m_qMenu->Add(new MenuBool("autoQ",   "Auto Q", true));
        m_qMenu->Add(new MenuBool("harassQ", "Harass Q", true));

        m_wMenu->Add(new MenuBool("autoW",      "Auto W", true));
        m_wMenu->Add(new MenuBool("autoWspeed", "W speed-up", true));

        Menu* wally = m_wMenu->AddSubMenu(new Menu("WallySub", "W ally:"));
        const auto myPlayer = Player();
        const auto myTeam = myPlayer.Team();
        for (const auto& ally : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!ally.IsValid() || ally.Team() != myTeam) continue;
            const std::string id = std::string("Wally") + ally.CharacterName();
            wally->Add(new MenuBool(id.c_str(), ally.CharacterName().c_str(), true));
        }

        m_eMenu->Add(new MenuBool("autoE",   "Auto E", true));
        m_eMenu->Add(new MenuBool("harassE", "Harass E", true));

        m_rMenu->Add(new MenuBool("autoR", "Auto R", true));
        Menu* rally = m_rMenu->AddSubMenu(new Menu("RallySub", "R ally:"));
        for (const auto& ally : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!ally.IsValid() || ally.Team() != myTeam) continue;
            const std::string id = std::string("Rally") + ally.CharacterName();
            rally->Add(new MenuBool(id.c_str(), ally.CharacterName().c_str(), true));
        }

        // TODO(oktw-port): "Spell Manager" per-enemy spell toggles rely on Spellbook.Spells enumeration

        m_farmMenu->Add(new MenuBool("farmE",   "Lane clear E",  true));
        m_farmMenu->Add(new MenuBool("jungleQ", "Jungle clear Q", true));
        m_farmMenu->Add(new MenuBool("jungleE", "Jungle clear E", true));

        // TODO(oktw-port): Obj_AI_Base.OnProcessSpellCast — auto R on ally under spell
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
        m_RMANA = 0.0f;
        if (!m_Q.IsReady()) {
            // C#: QMANA = QMANA - Player.PARRegenRate * Q.Instance.Cooldown; approximation kept flat
            // TODO(oktw-port): PARRegenRate/cooldown unavailable
        }
    }

    void OnGameUpdate() override {
        if (LagFree(1)) { SetMana(); Jungle(); }

        if (m_R.IsReady() && GetBool("autoR")) LogicR();

        const auto p = Player();
        if (LagFree(2) && m_W.IsReady() && !p.Spellbook().IsWindingUp() && GetBool("autoW"))
            LogicW();
        if (LagFree(3) && m_E.IsReady() && GetBool("autoE"))
            LogicE();
        if (LagFree(4) && m_Q.IsReady() && !p.Spellbook().IsWindingUp() && GetBool("autoQ"))
            LogicQ();
    }

    void LogicR() {
        const auto p = Player();
        const auto myTeam = p.Team();

        std::vector<AIHeroClient> allies;
        for (const auto& ally : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!ally.IsValid() || ally.IsDead()) continue;
            if (ally.Team() != myTeam) continue;
            if (ally.HealthPercent() >= 70.0f) continue;
            if (p.ServerPosition().Distance(ally.ServerPosition()) >= m_R.Range) continue;
            if (!GetBool((std::string("Rally") + ally.CharacterName()).c_str())) continue;
            allies.push_back(ally);
        }
        std::sort(allies.begin(), allies.end(),
            [](const AIHeroClient& a, const AIHeroClient& b) { return a.Health() < b.Health(); });

        for (const auto& ally : allies) {
            const float dmg = OktwCommon::GetIncomingDamage(ally);
            if (dmg == 0.0f) continue;
            if (ally.Health() - dmg < static_cast<float>(ally.Level()) * 12.0f)
                m_R.CastOnUnit(ally);
        }
    }

    void LogicQ() {
        auto* ts = SDK::TargetSelector::Instance();
        auto t = ts ? ts->GetTarget(m_Q.Range, DamageType::Magical) : AIHeroClient();
        const auto p = Player();

        if (t.IsValid()) {
            if (Combo()) {
                m_Q.Cast(t);
            } else if (Harass() && GetBool("harassQ") &&
                       GetBool((std::string("Harass") + t.CharacterName()).c_str()) &&
                       p.Mana() > m_RMANA + m_WMANA + m_QMANA + m_QMANA) {
                m_Q.Cast(t);
            } else if (p.Health() < static_cast<float>(p.Level()) * 40.0f &&
                       !m_W.IsReady() && !m_R.IsReady()) {
                m_Q.Cast(t);
            } else if (OktwCommon::GetKsDamage(t, m_Q) > t.Health()) {
                m_Q.Cast(t);
            }
        }
    }

    void LogicW() {
        const auto p = Player();
        if (false /* TODO(oktw-port): InFountain not available in SDK */ || p.HasBuff("Recall")) return;

        AIHeroClient lowest = p;
        const auto myTeam = p.Team();
        for (const auto& ally : SDK::ObjectManager::Get<AIHeroClient>()) {
            if (!ally.IsValid() || ally.IsDead()) continue;
            if (ally.Team() != myTeam) continue;
            if (!GetBool((std::string("Wally") + ally.CharacterName()).c_str())) continue;
            if (p.Position().Distance(ally.Position()) >= m_W.Range) continue;
            if (ally.Health() < lowest.Health()) lowest = ally;
        }

        if (p.Mana() > m_WMANA + m_QMANA &&
            lowest.Health() < static_cast<float>(lowest.Level()) * 40.0f) {
            m_W.CastOnUnit(lowest);
        } else if (p.Mana() > m_WMANA + m_EMANA + m_QMANA &&
                   lowest.Health() < lowest.MaxHealth() * 0.4f &&
                   lowest.Health() < 1500.0f) {
            m_W.CastOnUnit(lowest);
        } else if (p.Mana() > p.MaxMana() * 0.5f &&
                   lowest.Health() < lowest.MaxHealth() * 0.7f &&
                   lowest.Health() < 2000.0f) {
            m_W.CastOnUnit(lowest);
        } else if (p.Mana() > p.MaxMana() * 0.9f &&
                   lowest.Health() < lowest.MaxHealth() * 0.9f) {
            m_W.CastOnUnit(lowest);
        } else if (p.Mana() == p.MaxMana() &&
                   lowest.Health() < lowest.MaxHealth() * 0.9f) {
            m_W.CastOnUnit(lowest);
        }

        if (GetBool("autoWspeed")) {
            auto* ts = SDK::TargetSelector::Instance();
            auto t = ts ? ts->GetTarget(1000.0f, DamageType::Magical) : AIHeroClient();
            if (t.IsValid()) {
                if (Combo() && p.Mana() > m_WMANA + m_QMANA + m_EMANA &&
                    p.Position().Distance(t.Position()) > m_Q.Range) {
                    m_W.CastOnUnit(p);
                }
            }
        }
    }

    void LogicE() {
        const auto p = Player();
        if (Combo() && p.CountEnemyHeroesInRange(700.0f) > 0) {
            m_E.Cast();
        } else if (Harass() && GetBool("harassE") &&
                   p.Mana() > m_WMANA + m_EMANA + m_QMANA &&
                   p.CountEnemyHeroesInRange(500.0f) > 0) {
            m_E.Cast();
        } else if (FarmSpells() && GetBool("farmE") && FarmE()) {
            m_E.Cast();
        }
    }

    void Jungle() {
        const auto p = Player();
        if (LaneClear() && p.Mana() > m_RMANA + m_WMANA + m_RMANA + m_WMANA) {
            auto mobs = OktwCommon::GetMinions(p.ServerPosition(), 600.0f, false, true);
            if (mobs.empty()) return;
            const auto& mob = mobs.front();
            if (m_E.IsReady() && GetBool("jungleE")) {
                m_E.Cast();
                return;
            }
            if (m_Q.IsReady() && GetBool("jungleQ")) {
                m_Q.Cast(mob);
                return;
            }
        }
    }

    bool FarmE() {
        const auto p = Player();
        return !OktwCommon::GetMinions(p.ServerPosition(), 600.0f).empty();
    }

    void OnGameDraw() override {
        // Simplified: rely on SDK draw utilities for range circles
    }
};

} } // namespace Plugins::OKTW
