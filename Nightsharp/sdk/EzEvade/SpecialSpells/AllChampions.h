#pragma once
#include "ChampionPlugin.h"
#include "../Spells/SpellDetector.h"
#include "../Spells/ObjectTracker.h"
#include "../Utils/MathUtilsEz.h"
#include "../Utils/EvadeUtils.h"
#include "../Utils/DelayAction.h"
#include "../Helpers/ObjectCache.h"
#include "../Helpers/Position.h"

// ============================================================================
// AllChampions.h — C++ port of EzEvade/SpecialSpells/AllChampions.cs (180 lines)
//   Handles trap spells, click-remove, and three-way spell splitting
// ============================================================================

namespace EzEvade {
namespace SpecialSpells {

class AllChampions : public ChampionPlugin {
public:
    static inline std::map<std::string, bool> pDict;

    void LoadSpecialSpell(SpellData& spellData) override {
        // C# line 26-30: register WndProc (click-remove) once
        if (pDict.find("Game_OnWndProc") == pDict.end()) {
            // WndProc registration — handled externally via hook
            pDict["Game_OnWndProc"] = true;
        }

        // C# line 32-36: register OnCreate for traps
        if (spellData.hasTrap && pDict.find("GameObject_OnCreate") == pDict.end()) {
            pDict["GameObject_OnCreate"] = true;
        }

        // C# line 38-42: register OnDelete for traps
        if (spellData.hasTrap && pDict.find("GameObject_OnDelete") == pDict.end()) {
            pDict["GameObject_OnDelete"] = true;
        }

        // C# line 44-48: register OnUpdate for traps
        if (spellData.hasTrap && pDict.find("Game_OnUpdate") == pDict.end()) {
            pDict["Game_OnUpdate"] = true;
        }

        // C# line 50-54: register three-way processing
        if (spellData.isThreeWay && pDict.find("ProcessSpell_ProcessThreeWay") == pDict.end()) {
            SpellDetector::OnProcessSpecialSpell.push_back(
                [](SDK::GameObject* hero, const Vec3& start, const Vec3& end,
                   SpellData& sd, SpecialSpellEventArgs& specialArgs) {
                    ProcessSpell_ThreeWay(hero, start, end, sd, specialArgs);
                });
            pDict["ProcessSpell_ProcessThreeWay"] = true;
        }
    }

    // C# lines 57-90: OnCreate — handle trap objects
    static void OnCreateObject(SDK::GameObject* sender) {
        if (!sender) return;
        std::string objName = sender->GetChampionName();
        std::string lower = objName;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        SpellData spellData;
        if (SpellDetector::onProcessTraps.count(lower)) {
            SpellData trapData = SpellDetector::onProcessTraps[lower];
            if (trapData.spellName.find("_trap") == std::string::npos)
                trapData.spellName += "_trap";
            Vec3 pos = sender->GetPosition();
            SpellDetector::CreateSpellData(nullptr, pos, pos, trapData, sender, 1337.0f);
        }
    }

    // C# lines 92-123: OnDelete — handle trap removal
    static void OnDeleteObject(SDK::GameObject* sender) {
        if (!sender) return;
        std::string objName = sender->GetChampionName();
        std::string lower = objName;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        if (SpellDetector::onProcessTraps.count(lower)) {
            std::vector<int> toDelete;
            for (auto& entry : SpellDetector::detectedSpells) {
                std::string trapName = entry.second.info.trapBaseName;
                std::transform(trapName.begin(), trapName.end(), trapName.begin(), ::tolower);
                if (trapName == lower) {
                    toDelete.push_back(entry.first);
                    entry.second.spellObject = nullptr;
                }
            }
            for (int key : toDelete) {
                EzEvade::DelayAction::Add(1, [key]() { SpellDetector::DeleteSpell(key); });
            }
        }
    }

    // C# lines 125-141: OnUpdate — cleanup dead trap objects
    static void OnUpdate() {
        std::vector<int> toDelete;
        for (auto& entry : SpellDetector::detectedSpells) {
            if (entry.second.info.spellName.find("_trap") != std::string::npos) {
                if (entry.second.spellObject == nullptr) continue;
                if (!entry.second.spellObject->IsValid()) {
                    toDelete.push_back(entry.first);
                    entry.second.spellObject = nullptr;
                }
            }
        }
        for (int key : toDelete) {
            EzEvade::DelayAction::Add(1, [key]() { SpellDetector::DeleteSpell(key); });
        }
    }

    // C# lines 143-164: WndProc — click-to-remove skillshots
    static void OnWndProc(bool isLeftClick, const Vec2& cursorPos) {
        if (!ObjectCache::GetBool("ClickRemove")) return;
        if (!isLeftClick) return;

        std::vector<int> toDelete;
        for (auto& entry : SpellDetector::detectedSpells) {
            auto& spell = entry.second;
            if (Position::InSkillShot(cursorPos, spell, 50 + spell.info.radius)) {
                if (spell.info.range > 9000 || spell.info.spellName.find("_trap") != std::string::npos) {
                    toDelete.push_back(entry.first);
                }
            }
        }
        for (int key : toDelete) {
            EzEvade::DelayAction::Add(1, [key]() { SpellDetector::DeleteSpell(key); });
        }
    }

    // C# lines 166-176: ProcessSpell_ThreeWay
    static void ProcessSpell_ThreeWay(SDK::GameObject* hero, const Vec3& start,
        const Vec3& end, SpellData& spellData, SpecialSpellEventArgs& specialArgs)
    {
        if (spellData.isThreeWay) {
            Vec2 endPos2 = EzMathUtils::RotateVector(start.To2D(), end.To2D(), spellData.angle);
            SpellDetector::CreateSpellData(hero, start, Vec3(endPos2.x, 0, endPos2.y), spellData, nullptr, 0, false);

            Vec2 endPos3 = EzMathUtils::RotateVector(start.To2D(), end.To2D(), -spellData.angle);
            SpellDetector::CreateSpellData(hero, start, Vec3(endPos3.x, 0, endPos3.y), spellData, nullptr, 0, false);
        }
    }
};

} // namespace SpecialSpells
} // namespace EzEvade
