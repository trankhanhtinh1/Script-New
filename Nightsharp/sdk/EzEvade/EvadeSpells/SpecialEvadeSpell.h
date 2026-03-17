#pragma once
#include <string>
#include "EvadeSpellData.h"
#include "../Core/EvadeHelper.h"
#include "../../GameObjects/GameObjects.h"

namespace EzEvade {

    // Forward declarations
    class EvadeSpell;

    // =========================================================================
    // SpecialEvadeSpell
    //   C# original: ezEvade.SpecialEvadeSpell (SpecialEvadeSpell.cs)
    //   Contains champion-specific evade spell logic
    //   Line-by-line port preserving original logic
    // =========================================================================
    class SpecialEvadeSpell {
    public:
        // Load special spell handler for the given spell
        //   C# original: LoadSpecialSpell(EvadeSpellData spellData) (line 19-45)
        static void LoadSpecialSpell(EvadeSpellData& spellData);

        // --- Champion-specific UseSpell functions ---

        // Elise: Rappel (E)
        //   C# original: UseRappel(EvadeSpellData, bool) (line 47-62)
        static bool UseRappel(const EvadeSpellData& evadeSpell, bool process = true);

        // Nidalee: Pounce (W spider form)
        //   C# original: UsePounce(EvadeSpellData, bool) (line 64-77)
        static bool UsePounce(const EvadeSpellData& evadeSpell, bool process = true);

        // Riven: Broken Wings (Q)
        //   C# original: UseBrokenWings(EvadeSpellData, bool) (line 79-90)
        static bool UseBrokenWings(const EvadeSpellData& evadeSpell, bool process = false);

        // Ekko: Phase Dive E attack (E2)
        //   C# original: UseEkkoE2(EvadeSpellData, bool) (line 93-107)
        static bool UseEkkoE2(const EvadeSpellData& evadeSpell, bool process = true);

        // Ekko: Chronobreak (R)
        //   C# original: UseEkkoR(EvadeSpellData, bool) (line 109-127)
        static bool UseEkkoR(const EvadeSpellData& evadeSpell, bool process = true);
    };

} // namespace EzEvade
