#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <algorithm>

#include "EvadeSpellData.h"
#include "../Spells/Spell.h"
#include "../Helpers/ObjectCache.h"
#include "../Helpers/PositionInfo.h"
#include "../Helpers/EvadeCommand.h"
#include "../Helpers/Position.h"
#include "../Utils/EvadeUtils.h"
#include "../Utils/DelayAction.h"
#include "../../GameObjects/GameObjects.h"
#include "../../Game.h"

namespace EzEvade {

    // Proper includes instead of conflicting namespace forward declarations.
    // Situation, SpellDetector, EvadeHelper, Evade are defined in their own headers.
    // Include order matters - these are already included via the headers above.
    // We only need extern declarations for specific symbols used here.

    // Items namespace - for item usage
    //   C# original: Items.HasItem, Items.CanUseItem, Items.UseItem
    namespace Items {
        bool HasItem(int itemId);
        bool CanUseItem(int itemId);
        void UseItem(int itemId);
    }

    // =========================================================================
    // EvadeSpell class
    //   C# original: ezEvade.EvadeSpell (EvadeSpell.cs, 541 lines)
    //   Line-by-line port preserving original logic
    // =========================================================================
    class EvadeSpell {
    public:
        using Callback = std::function<void()>;

        static std::vector<EvadeSpellData> evadeSpells;        // C# line 20
        static std::vector<EvadeSpellData> itemSpells;         // C# line 21
        static EvadeCommand lastSpellEvadeCommand;             // C# line 22

        // Constructor: initializes the evade spell system
        //   C# original: EvadeSpell(Menu mainMenu) (line 27-38)
        EvadeSpell();

        // Check if hero is currently dashing from an evade spell
        //   C# original: CheckDashing() (line 45-55)
        static void CheckDashing();

        // Periodically check for new items
        //   C# original: CheckForItems() (line 57-73)
        static void CheckForItems();

        // Try to prefer an evade spell over movement
        //   C# original: PreferEvadeSpell() (line 114-133)
        static bool PreferEvadeSpell();

        // Main UseEvadeSpell logic
        //   C# original: UseEvadeSpell() (line 135-163)
        static void UseEvadeSpell();

        // Activate a specific evade spell against a threatening spell
        //   C# original: ActivateEvadeSpell(Spell spell, bool checkSpell) (line 165-387)
        static bool ActivateEvadeSpell(Spell& spell, bool checkSpell = false);

        // Execute the evade spell callback if process is true
        //   C# original: CastEvadeSpell(Callback, process) (line 389-395)
        static void CastEvadeSpell(Callback func, bool process = true);

        // Compare evade position option
        //   C# original: CompareEvadeOption(PositionInfo, checkSpell) (line 397-408)
        static bool CompareEvadeOption(PositionInfo* posInfo, bool checkSpell = false);

        // Determine if an evade spell should be activated
        //   C# original: ShouldActivateEvadeSpell(Spell) (line 410-441)
        static bool ShouldActivateEvadeSpell(Spell& spell);

        // Check if movement speed buff spell should be used
        //   C# original: ShouldUseMovementBuff(Spell) (line 443-460)
        static bool ShouldUseMovementBuff(Spell& spell);

        // Get the danger level of an evade spell from menu settings
        //   C# original: GetSpellDangerLevel(EvadeSpellData) (line 462-485)
        static int GetSpellDangerLevel(const EvadeSpellData& spell);

        // Get the summoner spell slot for a given spell name
        //   C# original: GetSummonerSlot(string) (line 487-499)
        SpellSlotId GetSummonerSlot(const std::string& spellName);

        // Load evade spell list for the local champion
        //   C# original: LoadEvadeSpellList() (line 501-538)
        void LoadEvadeSpellList();

    private:
        // Get default spell mode
        //   C# original: GetDefaultSpellMode(EvadeSpellData) (line 104-112)
        static int GetDefaultSpellMode(const EvadeSpellData& spell);

        // Create menu for an evade spell
        //   C# original: CreateEvadeSpellMenu(EvadeSpellData) (line 75-102)
        static void CreateEvadeSpellMenu(const EvadeSpellData& spell);
    };

} // namespace EzEvade
