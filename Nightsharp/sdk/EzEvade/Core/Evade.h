#pragma once
// =========================================================================
// Evade.h — C++ port of EzEvade/Core/Evade.cs (994 lines)
// Line-by-line, preserving original logic
// =========================================================================
#include <vector>
#include <map>
#include <string>
#include <cmath>
#include <cfloat>

#include "../Spells/Spell.h"
#include "../Spells/SpellData.h"
#include "../Helpers/ObjectCache.h"
#include "../Helpers/PositionInfo.h"
#include "../Helpers/EvadeCommand.h"
#include "../Helpers/Position.h"
#include "../Helpers/Situation.h"
#include "../Utils/EvadeUtils.h"
#include "../EvadeSpells/EvadeSpell.h"
#include "EvadeHelper.h"
#include "../Spells/SpellDetector.h"
#include "../../GameObjects/GameObjects.h"
#include "../../Game.h"
#include "EvadeState.h"

namespace EzEvade {

    // SpellDetector — full header now available
    // (forward declarations removed; see SpellDetector.h)

    // =========================================================================
    // Evade class — main evade controller
    //   C# original: ezEvade.Evade (Evade.cs, 994 lines)
    // =========================================================================
    class Evade {
    public:
        // =====================================================================
        // Static fields (C# lines 16-67)
        // =====================================================================
        // C# line 18
        // static SpellDetector* spellDetector; // handled by namespace
        // C# line 19-24 — testers etc, not needed in C++

        // C# line 26-27
        static inline int& lastSpellCast = EvadeState::lastSpellCast;
        static inline float& lastSpellCastTime = EvadeState::lastSpellCastTime;

        // C# line 29
        static inline float& lastWindupTime = EvadeState::lastWindupTime;

        // C# line 31-32
        static inline float lastTickCount = 0;
        static inline float lastStopEvadeTime = 0;

        // C# line 34-35
        static inline Vec3 lastMovementBlockPos = {0, 0, 0};
        static inline float lastMovementBlockTime = 0;

        // C# line 37-40
        static inline float lastEvadeOrderTime = 0;
        static inline float lastIssueOrderGameTime = 0;
        static inline float lastIssueOrderTime = 0;

        // C# line 42-44
        static inline Vec2 lastMoveToPosition = {0, 0};
        static inline Vec2 lastMoveToServerPos = {0, 0};
        static inline Vec2 lastStopPosition = {0, 0};

        // C# line 48-54
        static inline bool& isDodging = EvadeState::isDodging;
        static inline bool& dodgeOnlyDangerous = EvadeState::dodgeOnlyDangerous;
        static inline bool& devModeOn = EvadeState::devModeOn;
        static inline bool& hasGameEnded = EvadeState::hasGameEnded;
        static inline bool& isChanneling = EvadeState::isChanneling;
        static inline Vec2 channelPosition = {0, 0};

        // C# line 56
        static inline PositionInfo* lastPosInfo = nullptr;

        // C# line 58
        static inline EvadeCommand lastEvadeCommand;

        // C# line 60-61
        static inline EvadeCommand lastBlockedUserMoveTo;
        static inline float lastDodgingEndTime = 0;

        // C# line 65-67
        static inline float sumCalculationTime = 0;
        static inline float numCalculationTime = 0;
        static inline float avgCalculationTime = 0;

        // =====================================================================
        // Methods
        // =====================================================================

        // Constructor (C# line 69-72)
        Evade();

        // Init (C# line 79-210)
        void Initialize();

        // ResetConfig (C# line 212-270)
        static void ResetConfig(bool kappa = true);

        // Game_OnCastSpell (C# line 386-483)
        static void OnCastSpell(int spellSlot);

        // Game_OnIssueOrder (C# line 485-610)
        static void OnIssueOrder(int orderType, const Vec2& targetPos, SDK::GameObject* target);

        // Game_OnProcessSpell (C# line 620-651)
        static void OnProcessSpell(SDK::GameObject* hero, const std::string& spellName,
            int spellSlot, float castTime);

        // Game_OnGameUpdate (C# line 653-692)
        static void OnGameUpdate();

        // RecalculatePath (C# line 694-731)
        static void RecalculatePath();

        // ContinueLastBlockedCommand (C# line 733-757)
        static void ContinueLastBlockedCommand();

        // CheckHeroInDanger (C# line 759-793)
        static void CheckHeroInDanger();

        // DodgeSkillShots (C# line 795-867)
        static void DodgeSkillShots();

        // CheckLastMoveTo (C# line 869-885)
        static void CheckLastMoveTo();

        // isDodgeDangerousEnabled (C# line 887-902)
        static bool IsDodgeDangerousEnabled();

        // CheckDodgeOnlyDangerous (C# line 904-917)
        static void CheckDodgeOnlyDangerous();

        // SetAllUndodgeable (C# line 919-922)
        static void SetAllUndodgeable();

        // SpellDetector_OnProcessDetectedSpells (C# line 924-991)
        static void OnProcessDetectedSpells();
    };

} // namespace EzEvade
