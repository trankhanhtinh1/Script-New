#pragma once
#include "../IPlugin.h"
#include "sdk/Events/Gapcloser.h"
#include "sdk/GameObjects/GameObjects.h"
#include "sdk/GameObjects/GameObjectExtensions.h"
#include "sdk/UI/MenuUI.h"
#include "sdk/Utils/DashUtils.h"
#include "sdk/Utils/JungleUtils.h"
#include "sdk/Wrappers/Orbwalking/Orbwalker.h"
#include "sdk/Wrappers/Damages/DamageLibrary.h"
#include "sdk/Wrappers/Damages/DamageCalc.h"
#include "sdk/Wrappers/Spells/SpellCaster.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <memory>
#include <string.h>
#include <string>
#include <vector>

namespace Plugins {

    class EzrealPlugin : public IPlugin {
    public:
        const char* GetName() const override { return "Ezreal"; }
        const char* GetAuthor() const override { return "NightSharp"; }
        PluginCategory GetCategory() const override { return PluginCategory::Champion; }
        const char* GetRequiredChampion() const override { return "Ezreal"; }

        bool CanLoad() const override {
            return IsChampionMatch();
        }

        void OnLoad() override {
            if (m_menu) {
                return;
            }

            SetupSpells();
            BuildMenu();
            RegisterCallbacks();
        }

        void OnUnload() override {
            SDK::MenuUI::Menu::Remove("Ezreal");
            m_comboMenu.reset();
            m_harassMenu.reset();
            m_laneClearMenu.reset();
            m_jungleClearMenu.reset();
            m_rMenu.reset();
            m_miscMenu.reset();
            m_killStealMenu.reset();
            m_menu.reset();
        }

        void OnMenu() override {
            if (m_menu) {
                m_menu->Draw();
            }
        }

        void OnUpdate() override {
            if (!m_menu || !IsChampionMatch()) {
                return;
            }

            Game_OnUpdate();
        }

        SDK::MenuUI::Menu* GetMenuRoot() override {
            return m_menu.get();
        }

    private:
        std::shared_ptr<SDK::MenuUI::Menu> m_menu;
        std::shared_ptr<SDK::MenuUI::Menu> m_comboMenu;
        std::shared_ptr<SDK::MenuUI::Menu> m_harassMenu;
        std::shared_ptr<SDK::MenuUI::Menu> m_laneClearMenu;
        std::shared_ptr<SDK::MenuUI::Menu> m_jungleClearMenu;
        std::shared_ptr<SDK::MenuUI::Menu> m_rMenu;
        std::shared_ptr<SDK::MenuUI::Menu> m_miscMenu;
        std::shared_ptr<SDK::MenuUI::Menu> m_killStealMenu;
        bool m_callbacksRegistered = false;

        SDK::SpellCaster Q;
        SDK::SpellCaster W;
        SDK::SpellCaster E;
        SDK::SpellCaster R;
        SDK::SpellCaster EQ;
        SDK::SpellCaster Ignite;

    private:
        bool IsChampionMatch() const {
            const auto& player = SDK::GameObjects::Player;
            if (!player.IsValid()) {
                return false;
            }

            const std::string championName = player.GetChampionName();
            return championName.find("Ezreal") != std::string::npos;
        }

        void SetupSpells() {
            Q = SDK::SpellCaster(SDK::SpellSlotId::Q, 1200.0f);
            Q.SetSkillshot(
                0.25f,
                53.0f,
                2000.0f,
                SDK::CollisionMinions | SDK::CollisionHeroes,
                SDK::SpellCasterType::Line);

            W = SDK::SpellCaster(SDK::SpellSlotId::W, 1200.0f);
            W.SetSkillshot(
                0.25f,
                55.0f,
                1700.0f,
                SDK::CollisionNone,
                SDK::SpellCasterType::Line);

            E = SDK::SpellCaster(SDK::SpellSlotId::E, 475.0f);
            E.Delay = 0.65f;

            R = SDK::SpellCaster(SDK::SpellSlotId::R, 5000.0f);
            R.SetSkillshot(
                1.0f,
                160.0f,
                2200.0f,
                SDK::CollisionNone,
                SDK::SpellCasterType::Line);

            EQ = SDK::SpellCaster(SDK::SpellSlotId::Q, 1625.0f);
            EQ.SetSkillshot(
                0.90f,
                57.0f,
                1350.0f,
                SDK::CollisionMinions | SDK::CollisionHeroes,
                SDK::SpellCasterType::Line);

            const auto& player = SDK::GameObjects::Player;
            if (player.IsValid() && player.HasSummonerSpell(SDK::SummonerSpells::Ignite)) {
                Ignite = SDK::SpellCaster(player.GetSummonerSlot(SDK::SummonerSpells::Ignite), 600.0f);
            } else {
                Ignite = SDK::SpellCaster();
            }
        }

        void BuildMenu() {
            m_menu = SDK::MenuUI::Menu::Create("Ezreal", "Ezreal");

            m_comboMenu = m_menu->AddSubMenu("combo", "Combo");
            m_comboMenu->Add<SDK::MenuUI::MenuBool>("useQ", "Use Q", true);
            m_comboMenu->Add<SDK::MenuUI::MenuBool>("useW", "Use W", true);
            m_comboMenu->Add<SDK::MenuUI::MenuBool>("useE", "Use E", true);
            m_comboMenu->Add<SDK::MenuUI::MenuBool>("ComboECheck", "Use E |Safe Check", true);
            m_comboMenu->Add<SDK::MenuUI::MenuBool>("ComboEWall", "Use E |Wall Check", true);
            m_comboMenu->Add<SDK::MenuUI::MenuBool>("useR", "Use R", true);
            m_comboMenu->Add<SDK::MenuUI::MenuKeyBind>("SemiR", "Semi R", 'T', SDK::MenuUI::KeyBindType::Press);

            m_harassMenu = m_menu->AddSubMenu("harass", "Harass");
            m_harassMenu->Add<SDK::MenuUI::MenuBool>("useQ", "Use Q", true);
            m_harassMenu->Add<SDK::MenuUI::MenuBool>("useW", "Use W", true);

            m_laneClearMenu = m_menu->AddSubMenu("laneclear", "Lane Clear");
            m_laneClearMenu->Add<SDK::MenuUI::MenuBool>("useQ", "Use Q", true);
            m_laneClearMenu->Add<SDK::MenuUI::MenuBool>("QLH", "Use Q Last Hit", true);
            m_laneClearMenu->Add<SDK::MenuUI::MenuSlider>("ManaCL", "Mana Clear", 15, 0, 100);

            m_jungleClearMenu = m_menu->AddSubMenu("jungleclear", "Jungle Clear");
            m_jungleClearMenu->Add<SDK::MenuUI::MenuBool>("useQ", "Use Q", true);
            m_jungleClearMenu->Add<SDK::MenuUI::MenuBool>("useW", "Use W", true);
            m_jungleClearMenu->Add<SDK::MenuUI::MenuSlider>("ManaCL", "Mana Clear", 15, 0, 100);

            m_rMenu = m_menu->AddSubMenu("rmenu", "RMenu");
            m_rMenu->Add<SDK::MenuUI::MenuBool>("AutoR", "Auto R", true);
            m_rMenu->Add<SDK::MenuUI::MenuSlider>("RRange", "Auto R |Min Cast Range >= x", 900, 0, 1500);
            m_rMenu->Add<SDK::MenuUI::MenuSlider>("RMaxRange", "Auto R |Max Cast Range >= x", 3000, 1500, 5000);

            m_miscMenu = m_menu->AddSubMenu("misc", "Misc");
            m_miscMenu->Add<SDK::MenuUI::MenuBool>("gapcloser", "Gapcloser", true);

            m_killStealMenu = m_menu->AddSubMenu("killsteal", "KillSteal");
            m_killStealMenu->Add<SDK::MenuUI::MenuBool>("killstealQ", "Use Q", true);
        }

        void RegisterCallbacks() {
            if (m_callbacksRegistered) {
                return;
            }

            SDK::Orbwalker::OnBeforeAttack([this](SDK::OrbwalkingActionArgs& args) {
                if (!this->IsLoaded() || !this->IsEnabled() || !this->IsChampionMatch()) {
                    return;
                }
                this->OnBeforeAttack(args);
            });

            SDK::Orbwalker::OnAfterAttack([this](SDK::OrbwalkingActionArgs& args) {
                if (!this->IsLoaded() || !this->IsEnabled() || !this->IsChampionMatch()) {
                    return;
                }
                this->Orbwalker_OnAfterAttack(args);
            });

            SDK::Gapcloser::OnGapcloser([this](const SDK::GapcloserEventArgs& args) {
                if (!this->IsLoaded() || !this->IsEnabled() || !this->IsChampionMatch()) {
                    return;
                }
                this->Gapcloser_OnGapcloser(args);
            });

            SDK::EventSystem::OnBuffChanged([this](const SDK::BuffChangeArgs& args) {
                if (!this->IsLoaded() || !this->IsEnabled() || !this->IsChampionMatch() || !args.IsAdded) {
                    return;
                }
                this->OnBuffAdd(args);
            });

            m_callbacksRegistered = true;
        }

        void OnBeforeAttack(SDK::OrbwalkingActionArgs& args) {
            auto* useQItem = m_comboMenu ? m_comboMenu->Get<SDK::MenuUI::MenuBool>("useQ") : nullptr;
            auto* useWItem = m_comboMenu ? m_comboMenu->Get<SDK::MenuUI::MenuBool>("useW") : nullptr;
            auto* useEItem = m_comboMenu ? m_comboMenu->Get<SDK::MenuUI::MenuBool>("useE") : nullptr;
            auto* laneManaItem = m_laneClearMenu ? m_laneClearMenu->Get<SDK::MenuUI::MenuSlider>("ManaCL") : nullptr;

            const bool useQ = useQItem ? useQItem->Enabled : false;
            const bool useW = useWItem ? useWItem->Enabled : false;
            const bool useE = useEItem ? useEItem->Enabled : false;
            const int lanemana = laneManaItem ? laneManaItem->Value : 0;
            (void)useQ;
            (void)useE;
            (void)lanemana;

            if (!args.Target.IsValid() || args.Target.IsDead() || !args.Target.IsValidTarget() || args.Target.GetHealth() <= 0.0f) {
                return;
            }

            if (!args.Target.IsHero()) {
                return;
            }

            if (SDK::Orbwalker::ActiveMode != SDK::OrbwalkingMode::Combo) {
                return;
            }

            const SDK::GameObject& target = args.Target;
            if (!target.IsValid() || !target.IsValidTarget(W.Range)) {
                return;
            }

            if (!useW || !W.IsReady()) {
                return;
            }

            auto pred = W.GetPrediction(target);
            if ((int)pred.Hitchance >= (int)SDK::HitChance::High) {
                W.Cast(pred.CastPosition);
            }
        }

        void Orbwalker_OnAfterAttack(SDK::OrbwalkingActionArgs& args) {
            auto* useQItem = m_comboMenu ? m_comboMenu->Get<SDK::MenuUI::MenuBool>("useQ") : nullptr;
            auto* qHarassItem = m_harassMenu ? m_harassMenu->Get<SDK::MenuUI::MenuBool>("useQ") : nullptr;
            const bool useQ = useQItem ? useQItem->Enabled : false;
            const bool qHarass = qHarassItem ? qHarassItem->Enabled : false;

            if (!args.Target.IsValid() || args.Target.IsDead() || !args.Target.IsValidTarget() || args.Target.GetHealth() <= 0.0f) {
                return;
            }

            if (!args.Target.IsHero()) {
                return;
            }

            const SDK::GameObject& target = args.Target;
            if (!target.IsValid() || !target.IsValidTarget()) {
                return;
            }

            if (SDK::Orbwalker::ActiveMode == SDK::OrbwalkingMode::Combo) {
                if (useQ && Q.IsReady() && target.IsValidTarget(Q.Range)) {
                    auto qPred = Q.GetPrediction(target);
                    if ((int)qPred.Hitchance >= (int)SDK::HitChance::High) {
                        Q.Cast(qPred.CastPosition);
                    }
                }
            } else if (SDK::Orbwalker::ActiveMode == SDK::OrbwalkingMode::Harass ||
                       SDK::Orbwalker::ActiveMode == SDK::OrbwalkingMode::LaneClear) {
                if (qHarass && Q.IsReady() && target.IsValidTarget(Q.Range)) {
                    auto qPred = Q.GetPrediction(target);
                    if ((int)qPred.Hitchance >= (int)SDK::HitChance::High) {
                        Q.Cast(qPred.CastPosition);
                    }
                }
            }
        }

        void Gapcloser_OnGapcloser(const SDK::GapcloserEventArgs& args) {
            auto* gapcloserItem = m_miscMenu ? m_miscMenu->Get<SDK::MenuUI::MenuBool>("gapcloser") : nullptr;
            if (!gapcloserItem || !gapcloserItem->Enabled) {
                return;
            }

            const auto& sender = args.Sender;
            const auto& player = SDK::GameObjects::Player;
            if (!player.IsValid()) {
                return;
            }

            if (!E.IsReady() || !sender.IsValid() || !sender.IsValidTarget(E.Range)) {
                return;
            }

            const Vec3 playerPrevious = SDK::GameObjectExtensions::PreviousPosition(player);
            const Vec3 senderPrevious = SDK::GameObjectExtensions::PreviousPosition(sender);

            if (sender.IsMelee()) {
                const float checkRange = sender.GetAttackRange() + sender.GetBoundingRadius() + 100.0f;
                if (sender.IsValidTarget(checkRange)) {
                    E.Cast(playerPrevious.Extend(senderPrevious, -E.Range));
                }
            }

            if (sender.IsDashing()) {
                if (args.EndPos.Distance2D(player.GetPosition()) <= 250.0f ||
                    senderPrevious.Distance2D(player.GetPosition()) <= 300.0f) {
                    E.Cast(playerPrevious.Extend(senderPrevious, -E.Range));
                }
            }

            if (!SDK::GameObjectExtensions::IsCastingImporantSpell(sender)) {
                return;
            }

            if (senderPrevious.Distance2D(player.GetPosition()) <= 300.0f) {
                E.Cast(playerPrevious.Extend(senderPrevious, -E.Range));
            }
        }

        static bool EqualsIgnoreCase(const std::string& lhs, const char* rhs) {
            return rhs && _stricmp(lhs.c_str(), rhs) == 0;
        }

        static bool IsAntiHookBuffName(const std::string& buffName) {
            if (buffName.empty()) {
                return false;
            }

            // Hook-only whitelist.
            // Do not use generic CC types (stun/snare/knockup), because Ezreal E
            // cannot reliably break those once applied.
            return EqualsIgnoreCase(buffName, "ThreshQ") ||
                   EqualsIgnoreCase(buffName, "threshq") ||
                   EqualsIgnoreCase(buffName, "rocketgrab2") ||
                   EqualsIgnoreCase(buffName, "RocketGrab") ||
                   EqualsIgnoreCase(buffName, "PykeQ");
        }

        void OnBuffAdd(const SDK::BuffChangeArgs& args) {
            const auto& player = SDK::GameObjects::Player;
            if (!player.IsValid() || !args.Unit.IsValid() || !args.Unit.IsMe() || !E.IsReady()) {
                return;
            }

            if (!IsAntiHookBuffName(args.BuffName)) {
                return;
            }

            const Vec3 dashPos = SDK::DashUtils::CastDash(E, true);
            if (dashPos.IsValid() && !dashPos.IsZero()) {
                E.Cast(dashPos);
            }
        }

        void Game_OnUpdate() {
            const auto& player = SDK::GameObjects::Player;
            if (!player.IsValid() || player.IsDead()) {
                return;
            }

            const bool chatOpen = SDK::Game::IsChatOpen();
            const bool inputBlocked = !SDK::Game::IsGameFocused() || SDK::Game::IsShopOpen();
            const bool isRecalling = player.IsRecalling();

            // Manual actions are intentionally not blocked by recall/chat/input,
            // to avoid false positives breaking combo.
            const bool canManualCast = true;
            const bool canAutoCast = !chatOpen && !inputBlocked && !isRecalling;

            if (R.GetLevel() > 0) {
                if (m_rMenu) {
                    R.Range = (float)m_rMenu->GetSliderValue("RMaxRange", (int)R.Range);
                }
            }

            if (m_comboMenu && canManualCast) {
                auto semiR = m_comboMenu->Get<SDK::MenuUI::MenuKeyBind>("SemiR");
                if (semiR && semiR->Active) {
                    OneKeyCastR();
                }
            }

            if (m_rMenu && canAutoCast) {
                auto autoR = m_rMenu->Get<SDK::MenuUI::MenuBool>("AutoR");
                if (autoR && autoR->Enabled && R.IsReady() && SDK::GameObjects::CountEnemyHeroesInRange(1000.0f, player.GetPosition()) == 0) {
                    AutoRLogic();
                }
            }

            if (canManualCast) {
                switch (SDK::Orbwalker::ActiveMode) {
                    case SDK::OrbwalkingMode::Combo:
                        Combo();
                        return;
                    case SDK::OrbwalkingMode::Harass:
                        Harass();
                        break;
                    case SDK::OrbwalkingMode::LaneClear:
                        LaneClear();
                        JungleClear();
                        break;
                    case SDK::OrbwalkingMode::LastHit:
                        LastHit();
                        break;
                    default:
                        break;
                }
            }

            if (canAutoCast) {
                KillSteal();
            }
        }

        void OneKeyCastR() {
            if (!R.IsReady()) {
                return;
            }

            auto target = SDK::TargetSelector::GetTarget(R.Range, SDK::DamageType::Physical);
            const float rRangeVal = m_rMenu ? (float)m_rMenu->GetSliderValue("RRange", 900) : 900.0f;

            if (target.IsValidTarget(R.Range) && !target.IsValidTarget(rRangeVal)) {
                auto rPred = R.GetPrediction(target);
                if ((int)rPred.Hitchance >= (int)SDK::HitChance::High) {
                    R.Cast(rPred.CastPosition);
                }
            }
        }

        void AutoRLogic() {
            const auto& player = SDK::GameObjects::Player;
            const float rRangeVal = m_rMenu ? (float)m_rMenu->GetSliderValue("RRange", 900) : 900.0f;

            for (const auto& target : SDK::GameObjects::EnemyHeroes) {
                if (!target.IsValidTarget(R.Range) ||
                    target.GetPosition().Distance2D(player.GetPosition()) < rRangeVal) {
                    continue;
                }

                if (!target.CanMove() && target.IsValidTarget(EQ.Range) &&
                    SDK::DamageLibrary::GetSpellDamage(player, target, SDK::SpellSlotId::R) +
                    SDK::DamageLibrary::GetSpellDamage(player, target, SDK::SpellSlotId::Q) * 3.0 >
                    target.GetHealth() + target.GetHPRegen() * 2.0f) {
                    R.Cast(target);
                }

                if (SDK::DamageLibrary::GetSpellDamage(player, target, SDK::SpellSlotId::R) >
                    target.GetHealth() + target.GetHPRegen() * 2.0f &&
                    target.GetPathLength() < 2 &&
                    (int)R.GetPrediction(target).Hitchance >= (int)SDK::HitChance::High) {
                    R.Cast(target);
                }
            }
        }

        void Combo() {
            const auto& player = SDK::GameObjects::Player;
            auto target = SDK::TargetSelector::GetTarget(EQ.Range, SDK::DamageType::Physical);

            const bool useQ = m_comboMenu->Get<SDK::MenuUI::MenuBool>("useQ")->Enabled;
            const bool useW = m_comboMenu->Get<SDK::MenuUI::MenuBool>("useW")->Enabled;
            const bool useE = m_comboMenu->Get<SDK::MenuUI::MenuBool>("useE")->Enabled;
            const bool useR = m_comboMenu->Get<SDK::MenuUI::MenuBool>("useR")->Enabled;

            if (!target.IsValidTarget(EQ.Range)) {
                return;
            }

            if (useE && E.IsReady() && target.IsValidTarget(EQ.Range)) {
                ComboELogic(target);
            }

            if (useW && W.IsReady() && target.IsValidTarget(W.Range)) {
                const bool inAaRange = player.InAutoAttackRange(target);

                // In AA range: cast W directly, do not wait for Q.
                if (inAaRange) {
                    auto wPred = W.GetPrediction(target);
                    if ((int)wPred.Hitchance >= (int)SDK::HitChance::High) {
                        W.Cast(wPred.CastPosition);
                    }
                } else if (Q.IsReady() && target.IsValidTarget(Q.Range)) {
                    // Out of AA range: W waits for Q-ready condition.
                    auto qPred = Q.GetPrediction(target);
                    if ((int)qPred.Hitchance >= (int)SDK::HitChance::High) {
                        W.Cast(qPred.CastPosition);
                    }
                }
            }

            if (useQ && Q.IsReady() && target.IsValidTarget(Q.Range)) {
                auto qPred = Q.GetPrediction(target);
                if ((int)qPred.Hitchance >= (int)SDK::HitChance::High) {
                    Q.Cast(qPred.CastPosition);
                }
            }

            if (useR && R.IsReady()) {
                if (SDK::GameObjects::IsUnderEnemyTurret(player.GetPosition()) ||
                    SDK::GameObjects::CountEnemyHeroesInRange(800.0f, player.GetPosition()) > 1) {
                    return;
                }

                const float rRangeVal = m_rMenu ? (float)m_rMenu->GetSliderValue("RRange", 900) : 900.0f;
                for (const auto& rTarget : SDK::GameObjects::EnemyHeroes) {
                    if (!rTarget.IsValidTarget(R.Range) ||
                        rTarget.GetPosition().Distance2D(player.GetPosition()) < rRangeVal) {
                        continue;
                    }

                    if (rTarget.GetHealth() < SDK::DamageLibrary::GetSpellDamage(player, rTarget, SDK::SpellSlotId::R) &&
                        (int)R.GetPrediction(rTarget).Hitchance >= (int)SDK::HitChance::High &&
                        rTarget.GetPosition().Distance2D(player.GetPosition()) > Q.Range + E.Range / 2.0f) {
                        R.Cast(target);
                    }

                    if (rTarget.IsValidTarget(Q.Range + E.Range) &&
                        SDK::DamageLibrary::GetSpellDamage(player, rTarget, SDK::SpellSlotId::R) +
                        (Q.IsReady() ? SDK::DamageLibrary::GetSpellDamage(player, rTarget, SDK::SpellSlotId::Q) : 0.0) +
                        (W.IsReady() ? SDK::DamageLibrary::GetSpellDamage(player, rTarget, SDK::SpellSlotId::W) : 0.0) >
                        rTarget.GetHealth() + rTarget.GetHPRegen() * 2.0f) {
                        R.Cast(rTarget);
                    }
                }
            }
        }

        void ComboELogic(const SDK::GameObject& target) {
            const auto& player = SDK::GameObjects::Player;
            const bool eCheck = m_comboMenu->Get<SDK::MenuUI::MenuBool>("ComboECheck")->Enabled;
            const bool eWall = m_comboMenu->Get<SDK::MenuUI::MenuBool>("ComboEWall")->Enabled;

            if (!target.IsValid() || !target.IsValidTarget()) {
                return;
            }

            if (eCheck &&
                !SDK::GameObjects::IsUnderEnemyTurret(player.GetPosition()) &&
                SDK::GameObjects::CountEnemyHeroesInRange(1200.0f, player.GetPosition()) <= 2) {
                if (target.GetPosition().Distance2D(player.GetPosition()) > player.GetRealAutoAttackRange(&target) &&
                    target.IsValidTarget()) {
                    const Vec3 targetPrev = SDK::GameObjectExtensions::PreviousPosition(target);
                    const Vec3 playerPrev = SDK::GameObjectExtensions::PreviousPosition(player);
                    const Vec3 cursor = SDK::Game::GetMouseWorldPos();

                    if (target.GetHealth() <
                            SDK::DamageLibrary::GetSpellDamage(player, target, SDK::SpellSlotId::E) +
                            player.GetAutoAttackDamage(target) &&
                        targetPrev.Distance2D(cursor) < playerPrev.Distance2D(cursor)) {
                        const Vec3 castEPos = playerPrev.Extend(targetPrev, 475.0f);

                        if (eWall) {
                            if (!SDK::GameObject::IsWallAt(castEPos)) {
                                E.Cast(playerPrev.Extend(targetPrev, 475.0f));
                            }
                        } else {
                            E.Cast(playerPrev.Extend(targetPrev, 475.0f));
                        }
                        return;
                    }

                    if (target.GetHealth() <
                            SDK::DamageLibrary::GetSpellDamage(player, target, SDK::SpellSlotId::E) +
                            SDK::DamageLibrary::GetSpellDamage(player, target, SDK::SpellSlotId::W) &&
                        W.IsReady() &&
                        targetPrev.Distance2D(cursor) + 350.0f < playerPrev.Distance2D(cursor)) {
                        const Vec3 castEPos = playerPrev.Extend(targetPrev, 475.0f);

                        if (eWall) {
                            if (!SDK::GameObject::IsWallAt(castEPos)) {
                                E.Cast(playerPrev.Extend(targetPrev, 475.0f));
                            }
                        } else {
                            E.Cast(playerPrev.Extend(targetPrev, 475.0f));
                        }
                        return;
                    }

                    if (target.GetHealth() <
                            SDK::DamageLibrary::GetSpellDamage(player, target, SDK::SpellSlotId::E) +
                            SDK::DamageLibrary::GetSpellDamage(player, target, SDK::SpellSlotId::Q) &&
                        Q.IsReady() &&
                        targetPrev.Distance2D(cursor) + 300.0f < playerPrev.Distance2D(cursor)) {
                        const Vec3 castEPos = playerPrev.Extend(targetPrev, 475.0f);

                        if (eWall) {
                            if (!SDK::GameObject::IsWallAt(castEPos)) {
                                E.Cast(playerPrev.Extend(targetPrev, 475.0f));
                            }
                        } else {
                            E.Cast(playerPrev.Extend(targetPrev, 475.0f));
                        }
                    }
                }
            }
        }

        void Harass() {
            if (!m_harassMenu->GetBoolValue("useQ", false) || !Q.IsReady()) {
                return;
            }

            auto target = SDK::TargetSelector::GetTarget(Q.Range, SDK::DamageType::Physical);
            if (!target.IsValidTarget(Q.Range)) {
                return;
            }

            auto qPred = Q.GetPrediction(target);
            if ((int)qPred.Hitchance >= (int)SDK::HitChance::High) {
                Q.Cast(qPred.CastPosition);
            }
        }

        void LaneClear() {
            const auto& player = SDK::GameObjects::Player;
            if (!player.IsValid()) {
                return;
            }

            const bool useQ = m_laneClearMenu->GetBoolValue("useQ", false);
            const int mana = m_laneClearMenu->GetSliderValue("ManaCL", 15);
            if (!useQ || player.GetManaPercent() < (float)mana) {
                return;
            }

            if (!Q.IsReady()) {
                return;
            }

            // EnemyMinions is pre-filtered to Blue/Red team lane minions only
            auto minions = SDK::GameObjects::GetEnemyMinionsInRange(Q.Range, player.GetPosition());
            for (const auto& minion : minions) {
                if (!minion.IsValid() || !minion.IsAlive()) {
                    continue;
                }

                const float hpPred = Q.GetHealthPrediction(minion);
                float qDamage = Q.GetDamage(minion);
                if (qDamage <= 0.0f) {
                    qDamage = (float)QDamage(minion);
                }

                if (hpPred <= 0.0f || hpPred > qDamage) {
                    continue;
                }

                auto pred = Q.GetPrediction(minion, false, -1.0f, SDK::CollisionMinions);
                if ((int)pred.Hitchance >= (int)SDK::HitChance::Medium &&
                    pred.Hitchance != SDK::HitChance::Collision &&
                    pred.CastPosition.Distance2D(player.GetPosition()) <= Q.Range) {
                    Q.Cast(pred.CastPosition);
                    return;
                }
            }
        }

        void JungleClear() {
            const auto& player = SDK::GameObjects::Player;
            if (!player.IsValid()) {
                return;
            }
        
            const bool useQ = m_jungleClearMenu->GetBoolValue("useQ", false);
            const bool useW = m_jungleClearMenu->GetBoolValue("useW", false);
            const int mana = m_jungleClearMenu->GetSliderValue("ManaCL", 15);
        
            if (player.GetManaPercent() < (float)mana) {
                return;
            }
        
            // JungleMinions is filtered at source (GameObjects::Update) using RuntimeAPI
            // — guaranteed to contain only real jungle monsters, no plants/decorations.
            auto mobs = SDK::GameObjects::GetJungleMonstersInRange(Q.Range, player.GetPosition());
            if (mobs.empty())
                return;
        
            // Prioritize big monsters
            std::sort(mobs.begin(), mobs.end(),
                [](const SDK::GameObject& a, const SDK::GameObject& b) {
                    return SDK::JungleUtils::GetJungleType(a) >
                           SDK::JungleUtils::GetJungleType(b);
                });
        
            if (useW && W.IsReady()) {
                for (const auto& mob : mobs) {
                    if (!mob.IsValidTarget(W.Range))
                        continue;
        
                    if (SDK::JungleUtils::GetJungleType(mob) >= SDK::JungleType::Legendary) {
                        auto pred = W.GetPrediction(mob);
                        if ((int)pred.Hitchance >= (int)SDK::HitChance::High) {
                            W.Cast(pred.CastPosition);
                            break;
                        }
                    }
                }
            }
        
            if (useQ && Q.IsReady()) {
                const auto& mob = mobs.front();
                if (mob.IsValidTarget(Q.Range)) {
                    // Use prediction with collision check — Ezreal Q collides with minions
                    auto pred = Q.GetPrediction(mob, false, -1.0f, SDK::CollisionMinions);
                    if ((int)pred.Hitchance >= (int)SDK::HitChance::Medium &&
                        pred.Hitchance != SDK::HitChance::Collision) {
                        Q.Cast(pred.CastPosition);
                    }
                }
            }
        }

        void LastHit() {
            const auto& player = SDK::GameObjects::Player;
            if (!player.IsValid()) {
                return;
            }

            const int mana = m_laneClearMenu->GetSliderValue("ManaCL", 15);
            if (player.GetManaPercent() < (float)mana) {
                return;
            }

            if (!Q.IsReady()) {
                return;
            }

            auto minions = SDK::GameObjects::GetEnemyMinionsInRange(Q.Range, player.GetPosition());
            std::vector<SDK::GameObject> candidates;
            candidates.reserve(minions.size());

            // EnemyMinions is pre-filtered to Blue/Red team lane minions only
            for (const auto& minion : minions) {
                if (!minion.IsValidTarget(Q.Range) || !minion.IsMinion()) {
                    continue;
                }

                const float dist = minion.GetPosition().Distance2D(player.GetPosition());
                if (dist > Q.Range) {
                    continue;
                }

                if (dist <= player.GetRealAutoAttackRange(&minion)) {
                    continue;
                }

                const double qDamage = SDK::DamageLibrary::GetSpellDamage(player, minion, SDK::SpellSlotId::Q);
                if (minion.GetHealth() < qDamage) {
                    candidates.push_back(minion);
                }
            }

            if (!candidates.empty()) {
                Q.CastIfHitchanceEquals(candidates.front(), SDK::HitChance::Medium);
            }
        }

        double QDamage(const SDK::GameObject& target) const {
            const auto& player = SDK::GameObjects::Player;
            if (!player.IsValid() || !target.IsValid()) {
                return 0.0;
            }

            static const double qBaseDamage[6] = { 0.0, 20.0, 45.0, 70.0, 95.0, 120.0 };
            int qLevel = Q.GetLevel();
            if (qLevel < 0) qLevel = 0;
            if (qLevel > 5) qLevel = 5;

            const float rawDamage = (float)(qBaseDamage[qLevel] + 1.3 * (double)player.GetBonusAD());
            return (double)SDK::DamageCalc::CalcDamage(player, target, SDK::DamageType::Physical, rawDamage);
        }

        void KillSteal() {
            const auto& player = SDK::GameObjects::Player;
            if (!player.IsValid()) {
                return;
            }

            const bool ksQ = m_killStealMenu->GetBoolValue("killstealQ", false);
            if (!ksQ || !Q.IsReady()) {
                return;
            }

            for (const auto& target : SDK::GameObjects::EnemyHeroes) {
                if (!target.IsValidTarget(W.Range) ||
                    target.HasBuff("JudicatorIntervention") ||
                    target.HasBuff("kindredrnodeathbuff") ||
                    target.HasBuff("Undying Rage") ||
                    target.HasBuff("FioraW") ||
                    target.HasBuff("BlitzcrankManaBarrierCO")) {
                    continue;
                }

                const float distance = target.GetPosition().Distance2D(player.GetPosition());
                if (distance > 150.0f) {
                    if ((double)target.GetHealth() + (double)target.GetAllShield() <= QDamage(target)) {
                        auto qPred = Q.GetPrediction(target);
                        if ((int)qPred.Hitchance >= (int)SDK::HitChance::High) {
                            Q.Cast(qPred.CastPosition);
                        }
                    }
                } else {
                    if ((double)target.GetHealth() + (double)target.GetAllShield() <= QDamage(target) * 1.5) {
                        auto qPred = Q.GetPrediction(target);
                        if ((int)qPred.Hitchance >= (int)SDK::HitChance::High) {
                            Q.Cast(qPred.CastPosition);
                        }
                    }
                }
            }
        }
    };

} // namespace Plugins
