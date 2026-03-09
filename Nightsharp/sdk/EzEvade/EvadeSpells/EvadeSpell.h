#pragma once
#include "sdk/SDK.h"
#include "sdk/EzEvade/Core/EvadeHelper.h"
#include "sdk/EzEvade/EvadeSpells/EvadeSpellDatabase.h"
#include "sdk/EzEvade/EvadeSpells/SpecialEvadeSpell.h"
#include "sdk/EzEvade/Helpers/EvadeCommand.h"
#include "sdk/EzEvade/Helpers/EvadeContext.h"
#include "sdk/EzEvade/Helpers/ObjectCache.h"
#include "sdk/EzEvade/Helpers/Situation.h"
#include "sdk/EzEvade/Spells/SpellDetector.h"
#include "sdk/EzEvade/Utils/DelayAction.h"
#include <algorithm>
#include <string>
#include <vector>

namespace EzEvade {

class EvadeSpell {
public:
    static inline std::vector<EvadeSpellData> EvadeSpells = {};
    static inline std::vector<EvadeSpellData> ItemSpells = {};
    static inline std::shared_ptr<SDK::MenuUI::Menu> MenuRoot = nullptr;
    static inline std::shared_ptr<SDK::MenuUI::Menu> EvadeSpellMenu = nullptr;

    explicit EvadeSpell(const std::shared_ptr<SDK::MenuUI::Menu>& mainMenu) {
        MenuRoot = mainMenu;
        if (MenuRoot) {
            EvadeSpellMenu = MenuRoot->AddSubMenu("EvadeSpells", "Evade Spells");
        }

        LoadEvadeSpellList();
        DelayAction::Add(100, []() { CheckForItems(); });
    }

    static int GetDefaultSpellMode(const EvadeSpellData& spell) {
        return spell.dangerlevel > 3 ? 0 : 1;
    }

    static int GetSpellDangerLevel(const EvadeSpellData& spell) {
        auto* item = dynamic_cast<SDK::MenuUI::MenuList*>(ObjectCache::Menu.Get(spell.name + "EvadeSpellDangerLevel"));
        if (!item || item->Items.empty()) {
            return spell.dangerlevel;
        }

        const int idx = std::clamp(item->Index, 0, (int)item->Items.size() - 1);
        const std::string dangerStr = item->Items[(size_t)idx];
        if (_stricmp(dangerStr.c_str(), "Low") == 0) return 1;
        if (_stricmp(dangerStr.c_str(), "High") == 0) return 3;
        if (_stricmp(dangerStr.c_str(), "Extreme") == 0) return 4;
        return 2;
    }

    static bool ShouldUseMovementBuff(const Spell& spell) {
        const auto& me = SDK::GameObjects::Player;
        if (!me.IsValid()) return false;

        auto sorted = EvadeSpells;
        std::sort(sorted.begin(), sorted.end(), [](const EvadeSpellData& a, const EvadeSpellData& b) {
            return a.dangerlevel < b.dangerlevel;
        });

        for (const auto& evadeSpell : sorted) {
            if (evadeSpell.evadeType != EvadeType::MovementSpeedBuff) continue;

            if (!IsEvadeSpellEnabled(evadeSpell)) return false;
            if (GetSpellDangerLevel(evadeSpell) > SpellExtensions::GetSpellDangerLevel(spell)) return false;
            if (!CanUseEvadeSpellData(evadeSpell)) return false;
            if (evadeSpell.checkSpellName && !MatchSpellName(evadeSpell)) return false;
        }

        return true;
    }

    static bool PreferEvadeSpell() {
        if (!Situation::ShouldUseEvadeSpell()) return false;

        for (auto& [id, spell] : SpellDetector::Spells) {
            (void)id;
            if (Position::InSkillShot(ObjectCache::MyHeroCache.ServerPos2D, spell, ObjectCache::MyHeroCache.BoundingRadius)) {
                if (ActivateEvadeSpell(spell, true)) {
                    return true;
                }
            }
        }
        return false;
    }

    static void UseEvadeSpell() {
        if (!Situation::ShouldUseEvadeSpell()) {
            return;
        }

        if (SDK::Game::GetTickCount() - EvadeCommand::LastSpellEvadeCommand.Timestamp < 1000.0f) {
            return;
        }

        for (auto& [id, spell] : SpellDetector::Spells) {
            (void)id;
            if (ShouldActivateEvadeSpell(spell)) {
                if (ActivateEvadeSpell(spell)) {
                    EvadeContext::LastPosInfo = PositionInfo::SetAllUndodgeable();
                    EvadeContext::HasLastPosInfo = true;
                    return;
                }
            }
        }
    }

    static bool ActivateEvadeSpell(Spell& spell, bool checkSpell = false) {
        if (!spell.Info) return false;
        if (spell.Info->spellName.find("_trap") != std::string::npos) return false;

        std::vector<EvadeSpellData> sortedEvadeSpells = EvadeSpells;
        std::sort(sortedEvadeSpells.begin(), sortedEvadeSpells.end(), [](const EvadeSpellData& a, const EvadeSpellData& b) {
            return a.dangerlevel < b.dangerlevel;
        });

        const float extraDelayBuffer = (float)ObjectCache::Menu.GetSlider("ExtraPingBuffer", 65);
        const float spellActivationTime = (float)ObjectCache::Menu.GetSlider("SpellActivationTime", 400)
            + ObjectCache::GamePing + extraDelayBuffer;

        if (ObjectCache::Menu.GetBool("CalculateWindupDelay", true)) {
            const float extraWindupDelay = EvadeRuntimeState::LastWindupTime - (float)SDK::Game::GetTickCount();
            if (extraWindupDelay > 0.0f) {
                return false;
            }
        }

        const auto& me = SDK::GameObjects::Player;
        if (!me.IsValid()) return false;

        for (auto& evadeSpell : sortedEvadeSpells) {
            bool processSpell = true;

            if (!IsEvadeSpellEnabled(evadeSpell)
                || GetSpellDangerLevel(evadeSpell) > SpellExtensions::GetSpellDangerLevel(spell)
                || !CanUseEvadeSpellData(evadeSpell)
                || (evadeSpell.checkSpellName && !MatchSpellName(evadeSpell))) {
                continue;
            }

            float evadeTime = 0.0f;
            float spellHitTime = 0.0f;
            SpellExtensions::CanHeroEvade(spell, me, evadeTime, spellHitTime);
            const float finalEvadeTime = (spellHitTime - evadeTime);

            if (checkSpell) {
                const int mode = ObjectCache::Menu.GetListIndex(evadeSpell.name + "EvadeSpellMode", GetDefaultSpellMode(evadeSpell));
                if (mode == 0) {
                    continue;
                }
                if (mode == 1 && spellActivationTime < finalEvadeTime) {
                    continue;
                }
            } else {
                if (evadeSpell.spellDelay <= 50.0f && evadeSpell.evadeType != EvadeType::Dash) {
                    auto waypoints = me.GetWaypoints();
                    if (!waypoints.empty()) {
                        Vec2 movePos = waypoints.back().To2D();
                        PositionInfo posInfo = EvadeHelper::CanHeroWalkToPos(movePos, ObjectCache::MyHeroCache.MoveSpeed, 0.0f, 0.0f);
                        if (GetSpellDangerLevel(evadeSpell) > posInfo.PosDangerLevel) {
                            continue;
                        }
                    }
                }
            }

            if (evadeSpell.evadeType != EvadeType::Dash &&
                spellHitTime > evadeSpell.spellDelay + 100.0f + ObjectCache::GamePing + extraDelayBuffer) {
                processSpell = false;
                if (!checkSpell) {
                    continue;
                }
            }

            if (evadeSpell.isSpecial) {
                if (evadeSpell.useSpellFunc && evadeSpell.useSpellFunc(evadeSpell, processSpell)) {
                    return true;
                }
                continue;
            }

            if (evadeSpell.evadeType == EvadeType::Blink) {
                if (evadeSpell.castType == CastType::Position) {
                    auto posInfo = EvadeHelper::GetBestPositionBlink();
                    if (!posInfo.Position.IsZero()) {
                        CastEvadeSpell([&]() { EvadeCommand::CastSpell(evadeSpell, posInfo.Position); }, processSpell);
                        return true;
                    }
                } else if (evadeSpell.castType == CastType::Target) {
                    auto posInfo = EvadeHelper::GetBestPositionTargetedDash(evadeSpell);
                    if (posInfo.Target.IsValid() && posInfo.PosDangerLevel == 0) {
                        CastEvadeSpell([&]() { EvadeCommand::CastSpell(evadeSpell, posInfo.Target); }, processSpell);
                        return true;
                    }
                }
            } else if (evadeSpell.evadeType == EvadeType::Dash) {
                if (evadeSpell.castType == CastType::Position) {
                    auto posInfo = EvadeHelper::GetBestPositionDash(evadeSpell);
                    if (!posInfo.Position.IsZero() && CompareEvadeOption(posInfo, checkSpell)) {
                        if (evadeSpell.isReversed) {
                            Vec2 dir = (posInfo.Position - ObjectCache::MyHeroCache.ServerPos2D).Normalized();
                            float range = ObjectCache::MyHeroCache.ServerPos2D.Distance(posInfo.Position);
                            posInfo.Position = ObjectCache::MyHeroCache.ServerPos2D - dir * range;
                        }

                        CastEvadeSpell([&]() { EvadeCommand::CastSpell(evadeSpell, posInfo.Position); }, processSpell);
                        return true;
                    }
                } else if (evadeSpell.castType == CastType::Target) {
                    auto posInfo = EvadeHelper::GetBestPositionTargetedDash(evadeSpell);
                    if (posInfo.Target.IsValid() && posInfo.PosDangerLevel == 0) {
                        CastEvadeSpell([&]() { EvadeCommand::CastSpell(evadeSpell, posInfo.Target); }, processSpell);
                        return true;
                    }
                }
            } else if (evadeSpell.evadeType == EvadeType::WindWall) {
                if (SpellExtensions::HasProjectile(spell) || evadeSpell.spellName == "FioraW") {
                    Vec2 dir = (spell.StartPos - ObjectCache::MyHeroCache.ServerPos2D).Normalized();
                    Vec2 pos = ObjectCache::MyHeroCache.ServerPos2D + dir * 100.0f;
                    CastEvadeSpell([&]() { EvadeCommand::CastSpell(evadeSpell, pos); }, processSpell);
                    return true;
                }
            } else if (evadeSpell.evadeType == EvadeType::SpellShield || evadeSpell.evadeType == EvadeType::Shield || evadeSpell.evadeType == EvadeType::Stasis) {
                if (evadeSpell.isItem) {
                    CastEvadeSpell([&]() { SDK::Items::UseItem(evadeSpell.itemID); }, processSpell);
                    return true;
                }

                if (evadeSpell.castType == CastType::Target) {
                    CastEvadeSpell([&]() { EvadeCommand::CastSpell(evadeSpell, me); }, processSpell);
                    return true;
                }

                if (evadeSpell.castType == CastType::Self) {
                    CastEvadeSpell([&]() { EvadeCommand::CastSpell(evadeSpell); }, processSpell);
                    return true;
                }
            } else if (evadeSpell.evadeType == EvadeType::MovementSpeedBuff) {
                auto posInfo = EvadeHelper::GetBestPosition();
                if (posInfo.PosDangerCount >= 0) {
                    if (evadeSpell.isItem) {
                        CastEvadeSpell([&]() { SDK::Items::UseItem(evadeSpell.itemID); }, processSpell);
                    } else if (evadeSpell.castType == CastType::Self) {
                        CastEvadeSpell([&]() { EvadeCommand::CastSpell(evadeSpell); }, processSpell);
                    } else if (evadeSpell.castType == CastType::Position) {
                        CastEvadeSpell([&]() { EvadeCommand::CastSpell(evadeSpell, posInfo.Position); }, processSpell);
                    }

                    DelayAction::Add(5, [posInfo]() {
                        if (!posInfo.Position.IsZero()) {
                            EvadeCommand::MoveTo(posInfo.Position);
                        }
                    });
                    return true;
                }
            }
        }

        return false;
    }

    static void CastEvadeSpell(const std::function<void()>& func, bool process = true) {
        if (process && func) {
            func();
        }
    }

    static bool CompareEvadeOption(const PositionInfo& posInfo, bool checkSpell = false) {
        if (checkSpell && posInfo.PosDangerLevel == 0) {
            return true;
        }
        return PositionInfoExtensions::IsBetterMovePos(posInfo);
    }

private:
    static bool IsEvadeSpellEnabled(const EvadeSpellData& spell) {
        return ObjectCache::Menu.GetBool(spell.name + "UseEvadeSpell", true);
    }

    static SDK::SpellSlotId ToSdkSlot(SpellSlotId slot) {
        switch (slot) {
        case SpellSlotId::Q: return SDK::SpellSlotId::Q;
        case SpellSlotId::W: return SDK::SpellSlotId::W;
        case SpellSlotId::E: return SDK::SpellSlotId::E;
        case SpellSlotId::R: return SDK::SpellSlotId::R;
        case SpellSlotId::F: return SDK::SpellSlotId::Summoner1;
        case SpellSlotId::T: return SDK::SpellSlotId::Summoner2;
        default: return SDK::SpellSlotId::Q;
        }
    }

    static bool MatchSpellName(const EvadeSpellData& spell) {
        const auto& me = SDK::GameObjects::Player;
        if (!me.IsValid()) return false;
        const std::string currentName = me.GetSpell(ToSdkSlot(spell.spellKey)).GetName();
        return _stricmp(currentName.c_str(), spell.spellName.c_str()) == 0;
    }

    static bool CanUseEvadeSpellData(const EvadeSpellData& spell) {
        const auto& me = SDK::GameObjects::Player;
        if (!me.IsValid()) return false;

        if (spell.isItem) {
            return SDK::Items::CanUseItem(spell.itemID);
        }

        return me.GetSpell(ToSdkSlot(spell.spellKey)).IsReady();
    }

    static std::shared_ptr<SDK::MenuUI::Menu> CreateEvadeSpellMenu(const EvadeSpellData& spell) {
        if (!EvadeSpellMenu) return nullptr;
        const std::string menuName = spell.isItem
            ? (spell.name + " Settings")
            : (spell.name + " (" + std::to_string((int)spell.spellKey) + ") Settings");

        auto newSpellMenu = EvadeSpellMenu->AddSubMenu(spell.charName + spell.name + "EvadeSpellSettings", menuName);
        newSpellMenu->Add<SDK::MenuUI::MenuBool>(spell.name + "UseEvadeSpell", "Use Spell", true);
        newSpellMenu->Add<SDK::MenuUI::MenuList>(
            spell.name + "EvadeSpellDangerLevel",
            "Danger Level",
            std::vector<std::string>{ "Low", "Normal", "High", "Extreme" },
            std::clamp(spell.dangerlevel - 1, 0, 3));
        newSpellMenu->Add<SDK::MenuUI::MenuList>(
            spell.name + "EvadeSpellMode",
            "Spell Mode",
            std::vector<std::string>{ "Undodgeable", "Activation Time", "Always" },
            GetDefaultSpellMode(spell));

        return newSpellMenu;
    }

    static SpellSlotId GetSummonerSlot(const std::string& spellName) {
        const auto& me = SDK::GameObjects::Player;
        if (!me.IsValid()) return SpellSlotId::None;

        const std::string dName = me.GetSpell(SDK::SpellSlotId::Summoner1).GetName();
        const std::string fName = me.GetSpell(SDK::SpellSlotId::Summoner2).GetName();
        if (_stricmp(dName.c_str(), spellName.c_str()) == 0) return SpellSlotId::F;
        if (_stricmp(fName.c_str(), spellName.c_str()) == 0) return SpellSlotId::T;
        return SpellSlotId::None;
    }

    static bool ShouldActivateEvadeSpell(const Spell& spell) {
        if (!EvadeContext::HasLastPosInfo) return false;

        const bool dodgeSkillshotsActive = ObjectCache::Menu.GetKey("DodgeSkillShots", true);
        if (dodgeSkillshotsActive) {
            const bool inUndodgeable = std::find(
                EvadeContext::LastPosInfo.UndodgeableSpells.begin(),
                EvadeContext::LastPosInfo.UndodgeableSpells.end(),
                spell.SpellID) != EvadeContext::LastPosInfo.UndodgeableSpells.end();
            if (inUndodgeable && Position::InSkillShot(ObjectCache::MyHeroCache.ServerPos2D, spell, ObjectCache::MyHeroCache.BoundingRadius)) {
                return true;
            }
        } else {
            if (Position::InSkillShot(ObjectCache::MyHeroCache.ServerPos2D, spell, ObjectCache::MyHeroCache.BoundingRadius)) {
                return true;
            }
        }

        return false;
    }

    static void CheckForItems() {
        for (auto& spell : ItemSpells) {
            const bool hasItem = SDK::Items::HasItem(spell.itemID);
            const bool exists = std::any_of(EvadeSpells.begin(), EvadeSpells.end(), [&](const EvadeSpellData& s) {
                return s.spellName == spell.spellName;
            });

            if (hasItem && !exists) {
                EvadeSpells.push_back(spell);
                auto newSpellMenu = CreateEvadeSpellMenu(spell);
                ObjectCache::Menu.AddMenuToCache(newSpellMenu);
            }
        }

        DelayAction::Add(5000, []() { CheckForItems(); });
    }

    void LoadEvadeSpellList() {
        const auto& me = SDK::GameObjects::Player;
        if (!me.IsValid()) return;

        for (auto spell : GetEvadeSpellDatabase()) {
            if (!(spell.charName == me.GetChampionName() || spell.charName == "AllChampions")) {
                continue;
            }

            if (spell.isSummonerSpell) {
                SpellSlotId slot = GetSummonerSlot(spell.spellName);
                if (slot == SpellSlotId::None) {
                    continue;
                }
                spell.spellKey = slot;
            }

            if (spell.isItem) {
                ItemSpells.push_back(spell);
                continue;
            }

            if (spell.isSpecial) {
                SpecialEvadeSpell::LoadSpecialSpell(spell);
            }

            EvadeSpells.push_back(spell);
            auto newSpellMenu = CreateEvadeSpellMenu(spell);
            ObjectCache::Menu.AddMenuToCache(newSpellMenu);
        }

        std::sort(EvadeSpells.begin(), EvadeSpells.end(), [](const EvadeSpellData& a, const EvadeSpellData& b) {
            return a.dangerlevel < b.dangerlevel;
        });
    }
};

} // namespace EzEvade

