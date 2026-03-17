#include "EvadeCommand.h"
#include "Situation.h"
#include "../Core/EvadeState.h"
#include "../../Wrappers/Orbwalking/Orbwalker.h"
#include "../../Wrappers/Spells/SpellCaster.h"

// ============================================================================
// EvadeCommand — Implementation
//   C# original: ezEvade.EvadeCommand (EvadeCommand.cs, lines 39-113)
//
//   All static methods issue game commands (move/attack/cast).
//   Fully wired to NightSharp SDK APIs:
//     Orbwalker::IssueOrder  → move/attack
//     SpellCaster            → cast spells
// ============================================================================

namespace EzEvade {

    // Typed references into EvadeState storage
    static EvadeCommand& lastEvadeCommand =
        *reinterpret_cast<EvadeCommand*>(&EvadeState::lastEvadeCommandStorage);
    static EvadeCommand& lastSpellEvadeCommand =
        *reinterpret_cast<EvadeCommand*>(&EvadeState::lastSpellEvadeCommandStorage);

    // Helper: convert EzEvade SpellSlotId → SDK SpellSlotId
    static SDK::SpellSlotId ToSDKSlot(SpellSlotId slot) {
        switch (slot) {
            case SpellSlotId::Q: return SDK::SpellSlotId::Q;
            case SpellSlotId::W: return SDK::SpellSlotId::W;
            case SpellSlotId::E: return SDK::SpellSlotId::E;
            case SpellSlotId::R: return SDK::SpellSlotId::R;
            case SpellSlotId::F: return SDK::SpellSlotId::Summoner1;
            case SpellSlotId::T: return SDK::SpellSlotId::Summoner2;
            default:             return SDK::SpellSlotId::Q;
        }
    }

    // ========================================================================
    // MoveTo
    //   C# lines 39-58
    // ========================================================================
    void EvadeCommand::MoveTo(Vec2 movePos) {
        // C# line 41-44: if (!Situation.ShouldDodge()) return;
        if (!Situation::ShouldDodge()) return;

        // C# line 46-52: create EvadeCommand
        EvadeCommand cmd;
        cmd.order = EvadeOrderCommand::MoveTo;                      // C# line 48
        cmd.targetPosition = movePos;                                // C# line 49
        cmd.timestamp = EvadeUtils::TickCount();                     // C# line 50
        cmd.isProcessed = false;                                     // C# line 51

        // C# line 54-55: Store in Evade static fields
        lastEvadeCommand = cmd;
        EvadeState::lastMoveToPosition = movePos;
        EvadeState::lastMoveToServerPos =
            SDK::GameObjects::Player.GetPosition().To2D();

        // C# line 57: Player.IssueOrder(GameObjectOrder.MoveTo, movePos.To3D(), false)
        Vec3 movePos3D = Vec3(movePos.x, 0, movePos.y);
        SDK::Orbwalker::MoveTo(movePos3D);
    }

    // ========================================================================
    // Attack
    //   C# lines 60-72
    // ========================================================================
    void EvadeCommand::Attack(const EvadeSpellData& spellData, SDK::GameObject* target) {
        // C# line 62: EvadeSpell.lastSpellEvadeCommand = ...
        EvadeCommand cmd;
        cmd.order = EvadeOrderCommand::Attack;                       // C# line 64
        cmd.target = target;                                         // C# line 65
        cmd.evadeSpellData = spellData;                              // C# line 66
        cmd.timestamp = EvadeUtils::TickCount();                     // C# line 67
        cmd.isProcessed = false;                                     // C# line 68

        lastSpellEvadeCommand = cmd;

        // C# line 71: Player.IssueOrder(GameObjectOrder.AttackUnit, target, false)
        if (target && target->IsValid()) {
            SDK::Orbwalker::IssueOrder(3, target->GetPosition(), target); // 3 = AttackUnit
        }
    }

    // ========================================================================
    // CastSpell — target overload
    //   C# lines 74-86
    // ========================================================================
    void EvadeCommand::CastSpell(const EvadeSpellData& spellData, SDK::GameObject* target) {
        EvadeCommand cmd;
        cmd.order = EvadeOrderCommand::CastSpell;                    // C# line 78
        cmd.target = target;                                         // C# line 79
        cmd.evadeSpellData = spellData;                              // C# line 80
        cmd.timestamp = EvadeUtils::TickCount();                     // C# line 81
        cmd.isProcessed = false;                                     // C# line 82

        lastSpellEvadeCommand = cmd;

        // C# line 85: myHero.Spellbook.CastSpell(spellData.spellKey, target, false)
        if (target && target->IsValid()) {
            auto spell = SDK::SpellCaster::Targeted(
                ToSDKSlot(spellData.spellKey), spellData.range);
            spell.Cast(*target);
        }
    }

    // ========================================================================
    // CastSpell — position overload
    //   C# lines 88-100
    // ========================================================================
    void EvadeCommand::CastSpell(const EvadeSpellData& spellData, Vec2 movePos) {
        EvadeCommand cmd;
        cmd.order = EvadeOrderCommand::CastSpell;                    // C# line 92
        cmd.targetPosition = movePos;                                // C# line 93
        cmd.evadeSpellData = spellData;                              // C# line 94
        cmd.timestamp = EvadeUtils::TickCount();                     // C# line 95
        cmd.isProcessed = false;                                     // C# line 96

        lastSpellEvadeCommand = cmd;

        // C# line 99: myHero.Spellbook.CastSpell(spellData.spellKey, movePos.To3D(), false)
        Vec3 castPos = Vec3(movePos.x, 0, movePos.y);
        auto spell = SDK::SpellCaster::Targeted(
            ToSDKSlot(spellData.spellKey), spellData.range);
        spell.Cast(castPos);
    }

    // ========================================================================
    // CastSpell — self-cast overload
    //   C# lines 102-113
    // ========================================================================
    void EvadeCommand::CastSpell(const EvadeSpellData& spellData) {
        EvadeCommand cmd;
        cmd.order = EvadeOrderCommand::CastSpell;                    // C# line 106
        cmd.evadeSpellData = spellData;                              // C# line 107
        cmd.timestamp = EvadeUtils::TickCount();                     // C# line 108
        cmd.isProcessed = false;                                     // C# line 109

        lastSpellEvadeCommand = cmd;

        // C# line 112: myHero.Spellbook.CastSpell(spellData.spellKey, false)
        auto spell = SDK::SpellCaster::Targeted(
            ToSDKSlot(spellData.spellKey), 0);
        spell.Cast();
    }

} // namespace EzEvade
