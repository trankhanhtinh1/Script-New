#pragma once
#include "GameObject.h"
#include "SpellBook.h"
#include "GameObjects.h"
#include "Prediction.h"
#include "Game.h"
#include "Enums.h"
#include "../spoof/spoofcall.h"
#include <Psapi.h>
#include <string>

// ============================================================================
// SpellCaster — High-level spell casting with prediction
// Reference: EnsoulSharp.SDK/Core/Wrappers/Spells/Spell.cs
// ============================================================================

namespace SDK {

    // Spell caster type — simplified version for SpellCaster factory
    // (SpellType in Enums.h is the comprehensive EnsoulSharp version)
    enum class SpellCasterType {
        Targeted,
        Line,
        Circle,
        Cone,
        None
    };

    class SpellCaster {
    public:
        // Spell properties
        SpellSlotId Slot;
        SpellCasterType Type;
        float Range;
        float Speed;            // 0 = instant
        float Delay;            // Cast delay (seconds)
        float Width;            // Skillshot width
        int Collision;          // CollisionFlags
        bool IsCharged;
        float ChargeTime;
        float MinRange;

        // Constructors
        SpellCaster()
            : Slot(SpellSlotId::Q), Type(SpellCasterType::Targeted), Range(0),
              Speed(0), Delay(0.25f), Width(0), Collision(CollisionNone),
              IsCharged(false), ChargeTime(0), MinRange(0) {}

        SpellCaster(SpellSlotId slot, float range)
            : Slot(slot), Type(SpellCasterType::Targeted), Range(range),
              Speed(0), Delay(0.25f), Width(0), Collision(CollisionNone),
              IsCharged(false), ChargeTime(0), MinRange(0) {}

        // ====================================================================
        // Factory Methods
        // ====================================================================

        static SpellCaster Targeted(SpellSlotId slot, float range) {
            SpellCaster s;
            s.Slot = slot; s.Range = range; s.Type = SpellCasterType::Targeted;
            return s;
        }

        static SpellCaster Line(SpellSlotId slot, float range, float speed,
                                float width, float delay = 0.25f) {
            SpellCaster s;
            s.Slot = slot; s.Type = SpellCasterType::Line;
            s.Range = range; s.Speed = speed; s.Width = width; s.Delay = delay;
            return s;
        }

        static SpellCaster Circle(SpellSlotId slot, float range, float radius,
                                  float speed = 0, float delay = 0.25f) {
            SpellCaster s;
            s.Slot = slot; s.Type = SpellCasterType::Circle;
            s.Range = range; s.Speed = speed; s.Width = radius * 2; s.Delay = delay;
            return s;
        }

        static SpellCaster Cone(SpellSlotId slot, float range, float angle,
                                float delay = 0.25f) {
            SpellCaster s;
            s.Slot = slot; s.Type = SpellCasterType::Cone;
            s.Range = range; s.Width = angle; s.Delay = delay;
            return s;
        }

        // ====================================================================
        // Builder
        // ====================================================================
        SpellCaster& SetCollision(int flags) { Collision = flags; return *this; }
        SpellCaster& SetCharged(float time, float minR) {
            IsCharged = true; ChargeTime = time; MinRange = minR; return *this;
        }

        // ====================================================================
        // State
        // ====================================================================

        bool IsReady() const {
            SpellBook sb(GameObjects::Player.address);
            return sb.IsReady(Slot);
        }

        int GetLevel() const {
            SpellBook sb(GameObjects::Player.address);
            return sb.GetSpell(Slot).GetLevel();
        }

        float GetRemainingCD() const {
            SpellBook sb(GameObjects::Player.address);
            return sb.GetSpell(Slot).GetRemainingCooldown();
        }

        bool IsSkillshot() const {
            return Type != SpellCasterType::Targeted && Type != SpellCasterType::None;
        }

        bool InRange(const GameObject& target) const {
            return GameObjects::Player.DistanceTo(target) <= Range;
        }

        // ====================================================================
        // Cast Methods
        // ====================================================================

        // Self cast (no target)
        bool Cast() {
            if (!IsReady()) return false;
            CastSpellInternal(GameObjects::Player.GetPosition());
            return true;
        }

        // Cast on target (targeted spell)
        bool Cast(const GameObject& target) {
            if (!IsReady() || !target.IsValid()) return false;
            if (!InRange(target)) return false;

            if (IsSkillshot()) {
                return CastWithPrediction(target, HitChance::High);
            }

            // Targeted: cast at target position
            CastSpellInternal(target.GetPosition());
            return true;
        }

        // Cast at position
        bool Cast(const Vec3& pos) {
            if (!IsReady()) return false;
            CastSpellInternal(pos);
            return true;
        }

        // Cast with prediction + hit chance requirement
        bool CastWithPrediction(const GameObject& target,
                                HitChance minChance = HitChance::High) {
            if (!IsReady() || !target.IsValid()) return false;

            PredictionInput input;
            input.Range = Range;
            input.Speed = Speed;
            input.Delay = Delay;
            input.Width = Width;

            switch (Type) {
            case SpellCasterType::Line:   input.Type = SkillshotType::Line; break;
            case SpellCasterType::Circle: input.Type = SkillshotType::Circle; break;
            case SpellCasterType::Cone:   input.Type = SkillshotType::Cone; break;
            default: break;
            }

            auto pred = Prediction::GetPrediction(target, input);

            if ((int)pred.Hitchance < (int)minChance)
                return false;

            // Collision check
            Vec3 from = GameObjects::Player.GetPosition();
            if (Collision & CollisionMinions) {
                if (Collisions::HasMinionCollision(from, pred.CastPosition, Width))
                    return false;
            }
            if (Collision & CollisionHeroes) {
                if (Collisions::HasHeroCollision(from, pred.CastPosition, Width, target))
                    return false;
            }
            if (Collision & CollisionYasuoWall) {
                if (Collisions::HasYasuoWindWallCollision(from, pred.CastPosition))
                    return false;
            }

            CastSpellInternal(pred.CastPosition);
            return true;
        }

        // ====================================================================
        // Prediction helpers
        // ====================================================================

        PredictionResult GetPrediction(const GameObject& target) const {
            PredictionInput input;
            input.Range = Range;
            input.Speed = Speed;
            input.Delay = Delay;
            input.Width = Width;
            switch (Type) {
            case SpellCasterType::Line:   input.Type = SkillshotType::Line; break;
            case SpellCasterType::Circle: input.Type = SkillshotType::Circle; break;
            case SpellCasterType::Cone:   input.Type = SkillshotType::Cone; break;
            default: break;
            }
            return Prediction::GetPrediction(target, input);
        }

        // Get travel time to target
        float GetTravelTime(const GameObject& target) const {
            float dist = GameObjects::Player.DistanceTo(target);
            if (Speed <= 0) return Delay;
            return Delay + dist / Speed;
        }

        // ====================================================================
        // Cast at mouse position (convenience)
        // ====================================================================
        bool CastAtMouse() {
            LastCastTime = Game::GetTime();
            if (!IsReady()) {
                LastCastResult = -10; LastCastError = "Spell not ready";
                return false;
            }
            Vec3 mousePos = Game::GetMouseWorldPos();
            if (mousePos.IsZero()) {
                LastCastResult = -11; LastCastError = "Mouse pos zero";
                return false;
            }
            CastSpellInternal(mousePos);
            return true;
        }

        // ====================================================================
        // Cast via keypress simulation (Method 3 - most reliable for testing)
        // Reference: leagueoflegends-master guide castspell.md Method 3
        // Writes target to HUD mouse → simulates key press
        // ====================================================================
        bool CastAtMouseViaKey() {
            LastCastTime = Game::GetTime();
            if (!IsReady()) {
                LastCastResult = -10; LastCastError = "Spell not ready";
                return false;
            }

            Vec3 mousePos = Game::GetMouseWorldPos();
            if (mousePos.IsZero()) {
                LastCastResult = -11; LastCastError = "Mouse pos zero";
                return false;
            }

            // Get key for slot
            BYTE key = 0;
            switch (Slot) {
                case SpellSlotId::Q: key = 'Q'; break;
                case SpellSlotId::W: key = 'W'; break;
                case SpellSlotId::E: key = 'E'; break;
                case SpellSlotId::R: key = 'R'; break;
                default: {
                    LastCastResult = -12; LastCastError = "Invalid slot for key";
                    return false;
                }
            }

            // Find game window
            HWND gameWnd = FindWindowA("RiotWindowClass", nullptr);
            if (!gameWnd) gameWnd = FindWindowA(nullptr, "League of Legends (TM) Client");
            if (!gameWnd) {
                LastCastResult = -13; LastCastError = "Game window not found";
                return false;
            }

            // Simulate keypress → game casts spell at current mouse position
            PostMessageA(gameWnd, WM_KEYDOWN, key, 0);
            PostMessageA(gameWnd, WM_KEYUP, key, 0);

            LastCastResult = 1;
            LastCastError = "OK (KeySim)";
            return true;
        }

        // ====================================================================
        // Debug: Last cast result (for overlay)
        // ====================================================================
        static inline int    LastCastResult  = 0;   // 0=none, 1=success, -1..-9=error
        static inline float  LastCastTime    = 0.0f;
        static inline const char* LastCastError = "";

    private:
        // ====================================================================
        // Find trampoline gadget for spoof_call (cached, FF 23 = jmp [rbx])
        // ====================================================================
        static void* GetTrampoline() {
            static void* trampoline = nullptr;
            if (!trampoline) {
                MODULEINFO modInfo{};
                GetModuleInformation(GetCurrentProcess(),
                    (HMODULE)GetModuleHandleA(nullptr), &modInfo, sizeof(modInfo));
                char* base = (char*)GetModuleHandleA(nullptr);
                for (size_t i = 0; i < modInfo.SizeOfImage - 2; i++) {
                    if (base[i] == '\xFF' && base[i + 1] == '\x23') {
                        trampoline = base + i;
                        break;
                    }
                }
            }
            return trampoline;
        }

        // ====================================================================
        // Internal: Cast spell via CastSpellSafe (0xBB9DE0)
        //
        // IDA MCP analysis of sub_BB9DE0:
        //   - RCX = HudSpellInfo (from HudInstance + 0x68)
        //   - RDX = SpellInfo ptr (from SpellSlot + SlotSpellInfo)
        //   - Internally: searches spell slots via RDX, reads positions
        //     from SpellInput vtable calls, sets CastSpell flag, sends packet
        //
        // Flow:
        //   1. Write target pos to SpellInput (positions read via vtable)
        //   2. Write target pos to HUD mouse (fallback reads)
        //   3. Call CastSpellSafe(hudSpellInfo, spellInfoPtr) via spoof_call
        //   4. Restore original values
        // ====================================================================
        void CastSpellInternal(const Vec3& pos) {
            void* trampoline = GetTrampoline();
            if (!trampoline) {
                LastCastResult = -1; LastCastError = "No trampoline (FF 23)";
                return;
            }

            auto& player = GameObjects::Player;
            if (!player.IsValid()) {
                LastCastResult = -2; LastCastError = "Player invalid";
                return;
            }

            // Get SpellBook → SpellSlot
            uintptr_t spellBookAddr = player.address + Offset::SpellBook::Offset;
            uintptr_t spellSlotAddr = Globals::Read<uintptr_t>(
                spellBookAddr + Offset::SpellBook::SpellSlotArray + (int)Slot * 8);
            if (!Globals::IsValidPtr(spellSlotAddr)) {
                LastCastResult = -3; LastCastError = "SpellSlot invalid";
                return;
            }

            // Get SpellInfo + SpellInput pointers
            // Confirmed: SpellInfo=0x128 (SlotSpellInfo), SpellInput=0x120 (SlotSpellInput)
            uintptr_t spellInfoPtr = Globals::Read<uintptr_t>(
                spellSlotAddr + Offset::SpellBook::SlotSpellInfo);
            if (!Globals::IsValidPtr(spellInfoPtr)) {
                LastCastResult = -4; LastCastError = "SpellInfo invalid";
                return;
            }

            uintptr_t spellInput = Globals::Read<uintptr_t>(
                spellSlotAddr + Offset::SpellBook::SlotSpellInput);
            if (!Globals::IsValidPtr(spellInput)) {
                LastCastResult = -5; LastCastError = "SpellInput invalid";
                return;
            }

            // Get HudInstance → HudSpellInfo (RCX param for CastSpellSafe)
            uintptr_t hudInstance = Globals::Read<uintptr_t>(
                Globals::base + Offset::Global::HudInstance);
            if (!Globals::IsValidPtr(hudInstance)) {
                LastCastResult = -6; LastCastError = "HudInstance invalid";
                return;
            }

            uintptr_t hudSpellInfo = Globals::Read<uintptr_t>(
                hudInstance + Offset::Hud::SpellInfo);
            if (!Globals::IsValidPtr(hudSpellInfo)) {
                LastCastResult = -7; LastCastError = "HudSpellInfo invalid";
                return;
            }

            Vec3 playerPos = player.GetPosition();

            // Save original SpellInput values (will restore after cast)
            Vec3 origStartPos = Globals::Read<Vec3>(
                spellInput + Offset::SpellBook::InputStartPos);
            Vec3 origEndPos = Globals::Read<Vec3>(
                spellInput + Offset::SpellBook::InputEndPos);
            Vec3 origEndPos2 = Globals::Read<Vec3>(
                spellInput + Offset::SpellBook::InputEndPos + sizeof(Vec3));
            Vec3 origEndPos3 = Globals::Read<Vec3>(
                spellInput + Offset::SpellBook::InputEndPos + sizeof(Vec3) * 2);

            // Write target position to SpellInput
            Globals::Write<Vec3>(spellInput + Offset::SpellBook::InputStartPos, playerPos);
            Globals::Write<Vec3>(spellInput + Offset::SpellBook::InputEndPos, pos);
            Globals::Write<Vec3>(spellInput + Offset::SpellBook::InputEndPos + sizeof(Vec3), pos);
            Globals::Write<Vec3>(spellInput + Offset::SpellBook::InputEndPos + sizeof(Vec3) * 2, pos);

            // Also write to HUD mouse position (game also reads from here)
            uintptr_t hudInput = Globals::Read<uintptr_t>(hudInstance + Offset::Hud::Input);
            Vec3 origMouse;
            bool savedMouse = false;
            if (Globals::IsValidPtr(hudInput)) {
                origMouse = Globals::Read<Vec3>(hudInput + Offset::Hud::MouseWorldPos);
                Globals::Write<Vec3>(hudInput + Offset::Hud::MouseWorldPos, pos);
                savedMouse = true;
            }

            // ============================================================
            // Set bypass flags BEFORE calling (LeagueChimera pattern)
            // CastSpellFlag = byte, set to 1 to allow spell cast
            // ============================================================
            Globals::Write<uint8_t>(Globals::base + Offset::Flag::CastSpell, 1);

            // ============================================================
            // Call CastSpellSafe(hudSpellInfo, spellInfoPtr) via spoof_call
            // Reference: leagueoflegends-master/global/functions.cpp line 229
            //   param1 = *(HudInstance + 0x68) = hudSpellInfo
            //   param2 = *(SpellSlot + SlotSpellInfo) = spellInfoPtr
            // ============================================================
            using fnCastSpell = void(__fastcall*)(uintptr_t, uintptr_t);
            fnCastSpell fn = reinterpret_cast<fnCastSpell>(
                Globals::base + Offset::Function::CastSpellSafe);

            __try {
                spoof_call(trampoline, fn, hudSpellInfo, spellInfoPtr);
                LastCastResult = 1;
                LastCastError = "OK (FnCall)";
            } __except(1) {
                LastCastResult = -8; LastCastError = "CastSpellSafe CRASHED";
            }

            // Reset CastSpell flag
            Globals::Write<uint8_t>(Globals::base + Offset::Flag::CastSpell, 0);

            // Restore original SpellInput values
            Globals::Write<Vec3>(spellInput + Offset::SpellBook::InputStartPos, origStartPos);
            Globals::Write<Vec3>(spellInput + Offset::SpellBook::InputEndPos, origEndPos);
            Globals::Write<Vec3>(spellInput + Offset::SpellBook::InputEndPos + sizeof(Vec3), origEndPos2);
            Globals::Write<Vec3>(spellInput + Offset::SpellBook::InputEndPos + sizeof(Vec3) * 2, origEndPos3);

            // Restore HUD mouse position
            if (savedMouse && Globals::IsValidPtr(hudInput)) {
                Globals::Write<Vec3>(hudInput + Offset::Hud::MouseWorldPos, origMouse);
            }

            LastCastTime = Game::GetTime();
        }
    };

    // ========================================================================
    // SpellFactory — Pre-defined spells for popular champions
    // ========================================================================
    namespace SpellFactory {
        // Ezreal
        inline SpellCaster EzrealQ() {
            return SpellCaster::Line(SpellSlotId::Q, 1150, 2000, 120, 0.25f)
                .SetCollision(CollisionMinions | CollisionHeroes);
        }
        inline SpellCaster EzrealW() {
            return SpellCaster::Line(SpellSlotId::W, 1150, 1700, 160, 0.25f);
        }
        inline SpellCaster EzrealR() {
            return SpellCaster::Line(SpellSlotId::R, 20000, 2000, 320, 1.0f);
        }

        // Lux
        inline SpellCaster LuxQ() {
            return SpellCaster::Line(SpellSlotId::Q, 1175, 1200, 70, 0.25f)
                .SetCollision(CollisionHeroes);
        }
        inline SpellCaster LuxE() {
            return SpellCaster::Circle(SpellSlotId::E, 1100, 310, 1200, 0.25f);
        }
        inline SpellCaster LuxR() {
            return SpellCaster::Line(SpellSlotId::R, 3340, 0, 110, 1.0f);
        }

        // Morgana
        inline SpellCaster MorganaQ() {
            return SpellCaster::Line(SpellSlotId::Q, 1175, 1200, 70, 0.25f)
                .SetCollision(CollisionMinions | CollisionHeroes);
        }

        // Jinx
        inline SpellCaster JinxW() {
            return SpellCaster::Line(SpellSlotId::W, 1450, 3300, 60, 0.6f)
                .SetCollision(CollisionMinions | CollisionHeroes);
        }
        inline SpellCaster JinxR() {
            return SpellCaster::Line(SpellSlotId::R, 25000, 1700, 140, 0.6f)
                .SetCollision(CollisionHeroes);
        }

        // Blitzcrank
        inline SpellCaster BlitzcrankQ() {
            return SpellCaster::Line(SpellSlotId::Q, 1150, 1800, 70, 0.25f)
                .SetCollision(CollisionMinions | CollisionHeroes);
        }

        // Thresh
        inline SpellCaster ThreshQ() {
            return SpellCaster::Line(SpellSlotId::Q, 1100, 1900, 70, 0.5f)
                .SetCollision(CollisionMinions | CollisionHeroes);
        }

        // Ahri
        inline SpellCaster AhriE() {
            return SpellCaster::Line(SpellSlotId::E, 975, 1550, 60, 0.25f)
                .SetCollision(CollisionMinions | CollisionHeroes);
        }

        // Brand
        inline SpellCaster BrandW() {
            return SpellCaster::Circle(SpellSlotId::W, 900, 250, 0, 0.85f);
        }
    }

} // namespace SDK
