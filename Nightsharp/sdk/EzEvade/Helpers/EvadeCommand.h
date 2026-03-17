#pragma once
#include <string>
#include "../../GameObjects/GameObjects.h"
#include "../../Math/MathUtils.h"
#include "../Utils/EvadeUtils.h"
#include "../EvadeSpells/EvadeSpellData.h"

// ============================================================================
// EvadeCommand
//   C# original: ezEvade.EvadeCommand (EvadeCommand.cs, 116 lines)
//   Line-by-line port preserving original logic
//
//   Represents a command issued by the evade system: move, attack, or cast spell.
// ============================================================================

namespace EzEvade {

    // Forward declarations
    class Evade;
    class EvadeSpell;

    // ========================================================================
    // EvadeOrderCommand enum
    //   C# lines 14-20
    // ========================================================================
    enum class EvadeOrderCommand {
        None     = 0,           // C# line 16
        MoveTo   = 1,           // C# line 17
        Attack   = 2,           // C# line 18
        CastSpell = 3           // C# line 19
    };

    // ========================================================================
    // EvadeCommand class
    //   C# lines 22-114
    // ========================================================================
    class EvadeCommand {
    public:
        // C# line 24: private static AIHeroClient myHero
        // → SDK::GameObjects::Player

        EvadeOrderCommand order = EvadeOrderCommand::None;      // C# line 26
        Vec2 targetPosition = { 0, 0 };                        // C# line 27
        SDK::GameObject* target = nullptr;                      // C# line 28
        float timestamp = 0;                                    // C# line 29
        bool isProcessed = false;                               // C# line 30
        EvadeSpellData evadeSpellData;                          // C# line 31

        // ====================================================================
        // Default constructor
        //   C# lines 33-37
        // ====================================================================
        EvadeCommand() {
            this->timestamp = EvadeUtils::TickCount();          // C# line 35
            this->isProcessed = false;                          // C# line 36
        }

        // ====================================================================
        // static MoveTo(movePos)
        //   C# lines 39-58
        //   Checks ShouldDodge, creates command, issues MoveTo order
        // ====================================================================
        static void MoveTo(Vec2 movePos);
        // Implementation: (in .cpp, requires Evade/Situation forward refs)
        //
        // C# logic:
        // if (!Situation.ShouldDodge()) return;
        //
        // Evade.lastEvadeCommand = new EvadeCommand {
        //     order = EvadeOrderCommand.MoveTo,
        //     targetPosition = movePos,
        //     timestamp = EvadeUtils.TickCount,
        //     isProcessed = false
        // };
        // Evade.lastMoveToPosition = movePos;
        // Evade.lastMoveToServerPos = myHero.ServerPosition.To2D();
        // Player.IssueOrder(GameObjectOrder.MoveTo, movePos.To3D(), false);

        // ====================================================================
        // static Attack(spellData, target)
        //   C# lines 60-72
        // ====================================================================
        static void Attack(const EvadeSpellData& spellData, SDK::GameObject* target);
        // C# logic:
        // EvadeSpell.lastSpellEvadeCommand = new EvadeCommand {
        //     order = EvadeOrderCommand.Attack,
        //     target = target,
        //     evadeSpellData = spellData,
        //     timestamp = EvadeUtils.TickCount,
        //     isProcessed = false
        // };
        // Player.IssueOrder(GameObjectOrder.AttackUnit, target, false);

        // ====================================================================
        // static CastSpell(spellData, target)  — target overload
        //   C# lines 74-86
        // ====================================================================
        static void CastSpell(const EvadeSpellData& spellData, SDK::GameObject* target);
        // C# logic:
        // EvadeSpell.lastSpellEvadeCommand = ...
        // myHero.Spellbook.CastSpell(spellData.spellKey, target, false);

        // ====================================================================
        // static CastSpell(spellData, movePos) — position overload
        //   C# lines 88-100
        // ====================================================================
        static void CastSpell(const EvadeSpellData& spellData, Vec2 movePos);
        // C# logic:
        // EvadeSpell.lastSpellEvadeCommand = ...
        // myHero.Spellbook.CastSpell(spellData.spellKey, movePos.To3D(), false);

        // ====================================================================
        // static CastSpell(spellData) — self-cast overload
        //   C# lines 102-113
        // ====================================================================
        static void CastSpell(const EvadeSpellData& spellData);
        // C# logic:
        // EvadeSpell.lastSpellEvadeCommand = ...
        // myHero.Spellbook.CastSpell(spellData.spellKey, false);
    };

} // namespace EzEvade
