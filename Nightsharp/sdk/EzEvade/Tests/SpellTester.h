#pragma once
#include "sdk/SDK.h"
#include "sdk/EzEvade/Helpers/ObjectCache.h"
#include "sdk/EzEvade/Spells/SpellDatabase.h"
#include "sdk/EzEvade/Spells/SpellDetector.h"
#include <algorithm>
#include <unordered_map>

namespace EzEvade {

class SpellTester {
public:
    SpellTester() {
        Menu = SDK::MenuUI::Menu::Create("DummySpellTester", "Spell Tester");
        SelectSpellMenu = Menu->AddSubMenu("SelectSpellMenu", "Select Spell");
        auto setPos = Menu->AddSubMenu("SetPositionMenu", "Set Spell Position");
        auto fire = Menu->AddSubMenu("FireDummySpellMenu", "Fire Dummy Spell");

        setPos->Add<SDK::MenuUI::MenuBool>("SetDummySpellStartPosition", "Set Start Position", false);
        setPos->Add<SDK::MenuUI::MenuBool>("SetDummySpellEndPosition", "Set End Position", false);
        fire->Add<SDK::MenuUI::MenuKeyBind>("FireDummySpell", "Fire Dummy Spell Key", 'O', SDK::MenuUI::KeyBindType::Press);
        fire->Add<SDK::MenuUI::MenuSlider>("SpellInterval", "Spell Interval", 2500, 0, 5000);

        LoadSpellDictionary();

        SDK::EventSystem::OnGameUpdate([](float) {
            OnGameUpdate();
        });
    }

private:
    static inline std::shared_ptr<SDK::MenuUI::Menu> Menu = nullptr;
    static inline std::shared_ptr<SDK::MenuUI::Menu> SelectSpellMenu = nullptr;
    static inline std::unordered_map<std::string, std::unordered_map<std::string, SpellData>> SpellCache = {};
    static inline Vec3 SpellStartPosition = Vec3();
    static inline Vec3 SpellEndPosition = Vec3();
    static inline float LastSpellFireTime = 0.0f;

    static void LoadSpellDictionary() {
        SpellCache.clear();
        for (const auto& spell : GetSpellDatabase()) {
            SpellCache[spell.charName][spell.spellName] = spell;
        }

        std::vector<std::string> heroes;
        heroes.reserve(SpellCache.size());
        for (const auto& kv : SpellCache) {
            heroes.push_back(kv.first);
        }
        std::sort(heroes.begin(), heroes.end());

        if (heroes.empty()) {
            heroes.push_back("None");
        }

        SelectSpellMenu->Add<SDK::MenuUI::MenuList>("DummySpellHero", "Hero", heroes, 0);

        const auto& firstHero = heroes.front();
        std::vector<std::string> spells;
        auto it = SpellCache.find(firstHero);
        if (it != SpellCache.end()) {
            for (const auto& kv : it->second) {
                spells.push_back(kv.first);
            }
            std::sort(spells.begin(), spells.end());
        }
        if (spells.empty()) {
            spells.push_back("None");
        }
        SelectSpellMenu->Add<SDK::MenuUI::MenuList>("DummySpellList", "Spell", spells, 0);
    }

    static void OnGameUpdate() {
        if (!Menu) {
            return;
        }

        const auto& me = SDK::GameObjects::Player;
        if (!me.IsValid()) {
            return;
        }

        if (SpellStartPosition.IsZero()) {
            SpellStartPosition = me.GetServerPosition();
        }
        if (SpellEndPosition.IsZero()) {
            SpellEndPosition = me.GetServerPosition() + Vec3(500.0f, 0.0f, 0.0f);
        }

        if (Menu->GetBoolValue("SetDummySpellStartPosition", false)) {
            if (auto* item = Menu->Get<SDK::MenuUI::MenuBool>("SetDummySpellStartPosition")) {
                item->Enabled = false;
            }
            SpellStartPosition = me.GetServerPosition();
        }

        if (Menu->GetBoolValue("SetDummySpellEndPosition", false)) {
            if (auto* item = Menu->Get<SDK::MenuUI::MenuBool>("SetDummySpellEndPosition")) {
                item->Enabled = false;
            }
            SpellEndPosition = me.GetServerPosition();
        }

        if (!Menu->GetKeyBindValue("FireDummySpell", false)) {
            return;
        }

        const float nowTick = (float)SDK::Game::GetTickCount();
        const float interval = (float)Menu->GetSliderValue("SpellInterval", 2500);
        if (nowTick - LastSpellFireTime < interval) {
            return;
        }

        const std::string hero = GetSelectedListValue("DummySpellHero");
        const std::string spell = GetSelectedListValue("DummySpellList");

        auto itHero = SpellCache.find(hero);
        if (itHero == SpellCache.end()) {
            return;
        }
        auto itSpell = itHero->second.find(spell);
        if (itSpell == itHero->second.end()) {
            return;
        }

        auto dummy = std::make_shared<SpellData>(itSpell->second);
        SpellDetector::LoadDummySpell(dummy);
        SpellDetector::CreateSpellData(me, SpellStartPosition, SpellEndPosition, dummy);
        LastSpellFireTime = nowTick;
    }

    static std::string GetSelectedListValue(const std::string& key) {
        auto* list = Menu->Get<SDK::MenuUI::MenuList>(key);
        if (!list || list->Items.empty()) {
            return "";
        }
        const int idx = std::clamp(list->Index, 0, (int)list->Items.size() - 1);
        return list->Items[(size_t)idx];
    }
};

} // namespace EzEvade
