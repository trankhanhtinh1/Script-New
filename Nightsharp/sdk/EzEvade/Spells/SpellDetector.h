#pragma once
// =========================================================================
// SpellDetector.h — C++ port of EzEvade/Spells/SpellDetector.cs (971 lines)
// Line-by-line, preserving original logic
// =========================================================================
#include <map>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include <cctype>

#include "Spell.h"
#include "SpellData.h"
#include "SpellDatabase.h"
#include "SpellWindupDatabase.h"
#include "../Helpers/ObjectCache.h"
#include "../Helpers/EvadeCommand.h"
#include "../Utils/EvadeUtils.h"
#include "../../GameObjects/GameObjects.h"
#include "../../GameObjects/ObjectManager.h"
#include "../../Game.h"

namespace EzEvade {

    // =========================================================================
    // SpecialSpellEventArgs (C# line 16-20)
    // =========================================================================
    struct SpecialSpellEventArgs {
        bool noProcess = false;                                                     // C# line 18
        SpellData spellData;                                                        // C# line 19
    };

    // =========================================================================
    // SpellDetector — main spell detection engine
    //   C# original: ezEvade.SpellDetector (SpellDetector.cs, 971 lines)
    //   Converted to namespace with static functions (no menu system in C++)
    // =========================================================================
    namespace SpellDetector {

        // =====================================================================
        // Static data (C# lines 33-51)
        // =====================================================================
        extern std::map<int, Spell> spells;                                         // C# line 33
        extern std::map<int, Spell> drawSpells;                                     // C# line 34
        extern std::map<int, Spell> detectedSpells;                                 // C# line 35

        extern std::map<std::string, std::string> channeledSpells;                  // C# line 39

        extern std::map<std::string, SpellData> onProcessTraps;                     // C# line 41
        extern std::map<std::string, SpellData> onProcessSpells;                    // C# line 42
        extern std::map<std::string, SpellData> onMissileSpells;                    // C# line 43

        extern std::map<std::string, SpellData> windupSpells;                       // C# line 45

        extern int spellIDCount;                                                    // C# line 47

        extern float lastCheckTime;                                                 // C# line 51
        extern float lastCheckSpellCollisionTime;                                   // C# line 52

        // =====================================================================
        // Callback types (C# line 24-29)
        // =====================================================================
        using OnProcessDetectedSpellsHandler = std::function<void()>;               // C# line 24
        using OnProcessSpecialSpellHandler =                                        // C# line 27-28
            std::function<void(SDK::GameObject*, const Vec3&, const Vec3&, SpellData&, SpecialSpellEventArgs&)>;

        extern std::vector<OnProcessDetectedSpellsHandler> onProcessDetectedSpellsCallbacks;
        extern std::vector<OnProcessSpecialSpellHandler> OnProcessSpecialSpell;

        // =====================================================================
        // Methods
        // =====================================================================

        // Initialize (C# line 58-77) — loads spell database, channeled spells
        void Initialize();

        // SpellMissile_OnCreate (C# line 79-140)
        void OnMissileCreate(SDK::GameObject& missile, const Vec3& startPos,
            const Vec3& endPos, const std::string& missileName, int casterNetId, bool casterVisible);

        // SpellMissile_OnDelete (C# line 142-157)
        void OnMissileDelete(int missileNetId);

        // RemoveNonDangerousSpells (C# line 159-166)
        void RemoveNonDangerousSpells();

        // Game_ProcessSpell (C# line 168-220)
        void OnProcessSpell(SDK::GameObject* hero, const Vec3& startPos,
            const Vec3& endPos, const std::string& spellName);

        // CreateSpellData (C# line 222-380)
        void CreateSpellData(SDK::GameObject* hero, const Vec3& spellStartPos, const Vec3& spellEndPos,
            SpellData spellData, SDK::GameObject* obj = nullptr, float extraEndTick = 0.0f,
            bool processSpell = true, SpellType spellType = SpellType::None,
            bool checkEndExplosion = true, float spellRadius = 0);

        // Game_OnGameUpdate (C# line 382-399)
        void OnGameUpdate();

        // UpdateSpells (C# line 401-407)
        void UpdateSpells();

        // CheckSpellEndTime (C# line 409-432)
        void CheckSpellEndTime();

        // CheckSpellCollision (C# line 434-457)
        void CheckSpellCollision();

        // CanHeroWalkIntoSpell (C# line 459-506)
        bool CanHeroWalkIntoSpell(Spell& spell);

        // AddDetectedSpells (C# line 508-602)
        void AddDetectedSpells();

        // CreateSpell (C# line 604-619)
        int CreateSpell(Spell& newSpell, bool processSpell = true);

        // DeleteSpell (C# line 621-626)
        void DeleteSpell(int spellID);

        // GetCurrentSpellID (C# line 628-631)
        int GetCurrentSpellID();

        // GetSpellList (C# line 633-644)
        std::vector<int> GetSpellList();

        // GetHighestDetectedSpellID (C# line 646-656)
        int GetHighestDetectedSpellID();

        // GetLowestEvadeTime (C# line 658-676)
        float GetLowestEvadeTime(Spell*& lowestSpell);

        // GetMostDangerousSpell (C# line 678-698)
        Spell* GetMostDangerousSpell(bool hasProjectile = false);

        // InitChannelSpells (C# line 700-720)
        void InitChannelSpells();

        // LoadSpellDictionary (C# line 810-968)
        void LoadSpellDictionary();

    } // namespace SpellDetector

} // namespace EzEvade
