#pragma once

// ============================================================================
// CoreObjects.h - Game-object wrappers + manager enumeration
// ----------------------------------------------------------------------------
// Port of `old source/core/CoreObjects.h` with the following design changes:
//
//   * Pure game-object taxonomy: object classification is name-based via
//     `CoreClassification::Classify()` — the old RuntimeAPI type-flag wrapper
//     is no longer used (per TODO.md 1.2.1).
//
//   * Liveness checks: `IsDead()` / `IsAlive()` resolve native helpers through
//     `CoreRuntime::ResolveRva()` against `ControlRuntime::IsAlive` and
//     `ClassificationRuntime::IsDead`. If neither is available we fall
//     back to the `All::Dead` byte at 0x250.
//
//   * `IsMissile()`: kept as a vtable match against the MissileClient type
//     helper (currently still at OLD build RVA 0x4264F0 — REVERIFY if the
//     game ships a new build and the check starts returning false for all
//     objects). Non-missiles are screened out earlier by classification
//     anyway, so a stale RVA degrades gracefully.
// ============================================================================

#include "CoreAi.h"
#include "CoreBuffs.h"
#include "CoreBypass.h"
#include "CoreClassification.h"
#include "CoreItem.h"
#include "CoreNavGrid.h"
#include "CoreRuntime.h"
#include "CoreSpellBook.h"
#include "CoreSpellCastInfo.h"
#include "Globals.h"
#include "Vector.h"
#include "offset.h"

#include <cstdint>
#include <cstring>

namespace CoreObjects {

    // ── ActionState bit flags ──
    // Match the semantics of the old flat `Offset::ActionState` shim. Kept
    // local because Nightsharp `offset.h` only exposes the two word offsets
    // (ActionState1 / ActionState2); the bit masks are CoreObjects-private.
    namespace ActionStateFlags {
        constexpr unsigned int CanAttack       = 0x00000001;
        constexpr unsigned int CanCast         = 0x00000002;
        constexpr unsigned int CanMove         = 0x00000004;
        constexpr unsigned int Immovable       = 0x00000008;
        constexpr unsigned int Charmed         = 0x00000080;
        constexpr unsigned int Feared          = 0x00000100;
        constexpr unsigned int Taunted         = 0x00000200;
        constexpr unsigned int Stunned         = 0x00002000;
        constexpr unsigned int Suppressed      = 0x00008000;
        constexpr unsigned int Grounded        = 0x00100000;
        constexpr unsigned int Stasis          = 0x00200000;
        constexpr unsigned int ImmobileMask    = Immovable | Stunned | Suppressed | Stasis;
        constexpr unsigned int ForcedMoveMask  = Charmed | Feared | Taunted;
        constexpr unsigned int NoDashMask      = Grounded;

        constexpr int BuffTypeStun             = 5;
        constexpr int BuffTypeSnare            = 12;
        constexpr int BuffTypeSuppression      = 20;
        constexpr int BuffTypeKnockup          = 26;
    }

    // TODO: move this RVA into `Offset::ControlRuntime` once the sig scan
    // identifies the current-build address of the MissileClient type helper.
    constexpr uintptr_t kMissileTypeRVA = 0x4264F0;

    struct ManagerView {
        uintptr_t manager = 0;
        uintptr_t items = 0;
        int count = 0;

        bool IsValid() const {
            return Globals::IsValidPtr(manager) &&
                   Globals::IsValidPtr(items) &&
                   count > 0;
        }
    };

    // ─────────────────────────────────────────────────────────────────────
    // ObjectRef — thin wrapper around a raw game-object pointer.
    // Calls through to CoreAi / CoreBuffs / CoreItem / CoreSpellBook /
    // CoreSpellCastInfo for component-specific data; everything else reads
    // fields directly by offset.
    // ─────────────────────────────────────────────────────────────────────
    struct ObjectRef {
        uintptr_t address = 0;

        // ── Basic identity ──

        bool IsValid() const {
            return Globals::IsValidPtr(address);
        }

        int GetNetId() const {
            return Globals::Read<int>(address + Offset::All::NetId);
        }

        int GetIndex() const {
            return Globals::Read<int>(address + Offset::All::Index);
        }

        int GetTeam() const {
            return Globals::Read<unsigned char>(address + Offset::All::Team);
        }

        uint32_t GetChampionHash() const {
            if (!IsValid()) return 0;
            const auto charData = Globals::Read<uintptr_t>(address + Offset::All::CharacterData);
            if (!Globals::IsValidPtr(charData)) return 0;
            return Globals::Read<uint32_t>(charData + 0x10);  // CharacterHash (SDBM)
        }

        Vec3 GetPosition() const {
            return Globals::Read<Vec3>(address + Offset::All::Position);
        }

        Vec3 GetDirection() const {
            return Globals::Read<Vec3>(address + Offset::All::Direction);
        }

        // ── Health / mana / resources ──

        float GetHealth() const        { return Globals::Read<float>(address + Offset::AttackableUnit::HP); }
        float GetMaxHealth() const     { return Globals::Read<float>(address + Offset::AttackableUnit::MaxHP); }
        float GetMana() const          { return Globals::Read<float>(address + Offset::AIHeroClient::MP); }
        float GetMaxMana() const       { return Globals::Read<float>(address + Offset::AIHeroClient::MaxMP); }

        float GetAllShield() const      { return Globals::Read<float>(address + Offset::AttackableUnit::AllShield); }
        float GetPhysicalShield() const { return Globals::Read<float>(address + Offset::AttackableUnit::PhysicalShield); }
        float GetMagicalShield() const  { return Globals::Read<float>(address + Offset::AttackableUnit::MagicalShield); }

        float GetTotalShield() const    { return GetAllShield() + GetPhysicalShield() + GetMagicalShield(); }
        float GetEffectiveHealth() const { return GetHealth() + GetTotalShield(); }

        float GetHealthPercent() const {
            const float maxHealth = GetMaxHealth();
            return maxHealth > 0.0f ? (GetHealth() / maxHealth) * 100.0f : 0.0f;
        }

        float GetManaPercent() const {
            const float maxMana = GetMaxMana();
            return maxMana > 0.0f ? (GetMana() / maxMana) * 100.0f : 0.0f;
        }

        // ── Stats ──

        float GetMoveSpeed() const            { return Globals::Read<float>(address + Offset::AIHeroClient::MoveSpeed); }
        float GetHPRegenRate() const          { return Globals::Read<float>(address + Offset::AIHeroClient::HPRegenRate); }
        float GetBaseHPRegenRate() const      { return Globals::Read<float>(address + Offset::AIHeroClient::BaseHPRegenRate); }
        float GetAttackRange() const          { return Globals::Read<float>(address + Offset::AIHeroClient::AttackRange); }
        float GetBaseAD() const               { return Globals::Read<float>(address + Offset::AIHeroClient::BaseAttackDamage); }
        float GetBonusAD() const              { return Globals::Read<float>(address + Offset::AIHeroClient::FlatPhysicalDmgMod); }
        float GetTotalAD() const              { return GetBaseAD() + GetBonusAD(); }
        float GetAbilityPower() const         { return Globals::Read<float>(address + Offset::AIHeroClient::BaseAbilityDamage); }
        float GetArmor() const                { return Globals::Read<float>(address + Offset::AIHeroClient::Armor); }
        float GetBonusArmor() const           { return Globals::Read<float>(address + Offset::AIHeroClient::BonusArmor); }
        float GetSpellBlock() const           { return Globals::Read<float>(address + Offset::AIHeroClient::SpellBlock); }
        float GetBonusSpellBlock() const      { return Globals::Read<float>(address + Offset::AIHeroClient::BonusSpellBlock); }
        float GetCrit() const                 { return Globals::Read<float>(address + Offset::AIHeroClient::Crit); }
        float GetCritMultiplier() const       { return Globals::Read<float>(address + Offset::AIHeroClient::CritDamageMultiplier); }
        float GetAttackSpeedMod() const       { return Globals::Read<float>(address + Offset::AIHeroClient::AttackSpeedMod); }
        float GetPercentAttackSpeedMod() const{ return Globals::Read<float>(address + Offset::AIHeroClient::PercentAttackSpeedMod); }
        float GetFlatBaseAttackSpeedMod() const { return Globals::Read<float>(address + Offset::AIHeroClient::FlatBaseAttackSpeedMod); }
        float GetAbilityHaste() const         { return Globals::Read<float>(address + Offset::AIHeroClient::AbilityHaste); }
        float GetArmorPenFlat() const         { return Globals::Read<float>(address + Offset::AIHeroClient::FlatArmorPen); }
        float GetLethality() const            { return Globals::Read<float>(address + Offset::AIHeroClient::PhysicalLethality); }
        float GetArmorPenPercent() const      { return Globals::Read<float>(address + Offset::AIHeroClient::PercentArmorPen); }
        float GetBonusArmorPenPercent() const { return Globals::Read<float>(address + Offset::AIHeroClient::PercentBonusArmorPen); }
        float GetMagicPenFlat() const         { return Globals::Read<float>(address + Offset::AIHeroClient::FlatMagicPen); }
        float GetMagicPenPercent() const      { return Globals::Read<float>(address + Offset::AIHeroClient::PercentMagicPen); }
        float GetBonusMagicPenPercent() const { return Globals::Read<float>(address + Offset::AIHeroClient::PercentBonusMagicPen); }
        float GetLifeSteal() const            { return Globals::Read<float>(address + Offset::AIHeroClient::PercentLifeSteal); }
        float GetSpellVamp() const            { return Globals::Read<float>(address + Offset::AIHeroClient::PercentSpellVamp); }
        float GetOmnivamp() const             { return Globals::Read<float>(address + Offset::AIHeroClient::PercentOmnivamp); }
        float GetPhysDmgPercent() const       { return Globals::Read<float>(address + Offset::AIHeroClient::PhysDmgPercent); }
        float GetMagicDmgPercent() const      { return Globals::Read<float>(address + Offset::AIHeroClient::MagicDmgPercent); }

        float GetShutdownValue() const        { return Globals::Read<float>(address + Offset::AIHeroClient::ShutdownValue); }
        float GetBaseGoldOnDeath() const      { return Globals::Read<float>(address + Offset::AIHeroClient::BaseGoldOnDeath); }
        float GetNeutralMinionsKilled() const { return Globals::Read<float>(address + Offset::AIHeroClient::NeutralMinionsKilled); }

        int GetLevel() const          { return Globals::Read<int>(address + Offset::AIHeroClient::LevelRef); }
        int GetLevelUpPoints() const  { return Globals::Read<int>(address + Offset::AIHeroClient::LevelUpPoints); }
        float GetGold() const         { return Globals::Read<float>(address + Offset::AIHeroClient::Gold); }
        float GetGoldTotal() const    { return Globals::Read<float>(address + Offset::AIHeroClient::GoldTotal); }
        float GetExp() const          { return Globals::Read<float>(address + Offset::AIHeroClient::Exp); }

        // ── Bounding radius (native func + fallback) ──

        float GetBoundingRadius() const {
            const auto fn = CoreRuntime::ResolveRva(Offset::ControlRuntime::GetBoundingRadius);
            if (!fn) {
                return Globals::Read<float>(address + Offset::All::Radius);
            }

            using fnGetBoundingRadius = float(__fastcall*)(uintptr_t);
            __try {
                const float radius = reinterpret_cast<fnGetBoundingRadius>(fn)(address);
                if (radius > 0.0f && radius < 500.0f) {
                    return radius;
                }
            } __except (1) {}

            return Globals::Read<float>(address + Offset::All::Radius);
        }

        // ── Action state ──

        int GetActionState() const  { return Globals::Read<int>(address + Offset::AttackableUnit::ActionState1); }
        int GetActionState2() const { return Globals::Read<int>(address + Offset::AttackableUnit::ActionState2); }

        bool CanAttack() const       { return (GetActionState() & ActionStateFlags::CanAttack) != 0; }
        bool CanMove() const         { return (GetActionState() & ActionStateFlags::CanMove) != 0; }
        bool CanCast() const         { return (GetActionState() & ActionStateFlags::CanCast) != 0; }
        bool HasImmobileBuff() const {
            return CoreBuffs::HasBuffType(address, ActionStateFlags::BuffTypeStun) ||
                   CoreBuffs::HasBuffType(address, ActionStateFlags::BuffTypeSnare) ||
                   CoreBuffs::HasBuffType(address, ActionStateFlags::BuffTypeSuppression) ||
                   CoreBuffs::HasBuffType(address, ActionStateFlags::BuffTypeKnockup);
        }
        bool IsImmobile() const      {
            return (GetActionState() & ActionStateFlags::ImmobileMask) != 0 ||
                   HasImmobileBuff();
        }
        bool HasForcedMove() const   { return (GetActionState() & ActionStateFlags::ForcedMoveMask) != 0; }
        bool IsGrounded() const      { return (GetActionState() & ActionStateFlags::NoDashMask) != 0; }
        bool CanDash() const         { return !IsGrounded(); }
        bool IsCrowdControlled() const { return IsImmobile() || HasForcedMove(); }

        // ── Spell / cast shortcuts ──

        uintptr_t GetSpellBook() const {
            return CoreSpellBook::GetSpellBook(address);
        }

        CoreSpellBook::SlotRef GetSpellSlot(int slotId) const {
            return CoreSpellBook::GetSlot(address, slotId);
        }

        bool HasEnoughManaFor(int slotId) const {
            return CoreSpellBook::HasEnoughMana(address, slotId);
        }

        bool CanUseSpell(int slotId, float gameTime) const {
            return CoreSpellBook::CanCast(address, slotId, gameTime);
        }

        CoreSpellBook::SpellState GetSpellState(int slotId, float gameTime) const {
            return CoreSpellBook::GetSpellState(address, slotId, gameTime);
        }

        CoreSpellCastInfo::CastRef GetActiveSpellCast() const {
            return CoreSpellCastInfo::GetActive(address);
        }

        // ── AI / pathing shortcuts ──

        uintptr_t GetBuffManager() const       { return CoreBuffs::GetBuffManager(address); }
        CoreAi::ManagerRef GetAiManager() const { return CoreAi::Get(address); }

        bool HasPath() const         { return CoreAi::HasPath(address); }
        bool IsMovingOnPath() const  { return CoreAi::IsMoving(address); }
        bool IsDashingOnPath() const { return CoreAi::IsDashing(address); }
        bool HasArrived() const      { return CoreAi::HasArrived(address); }

        uint32_t GetDashTargetNetId() const  { return CoreAi::GetDashTargetNetId(address); }
        float GetDashDuration() const        { return CoreAi::GetDashDuration(address); }
        float GetDashDistRemaining() const   { return CoreAi::GetDashDistRemaining(address); }
        int GetCurrentPathSegment() const    { return CoreAi::GetCurrentSegment(address); }

        Vec3 GetVelocity() const       { return CoreAi::GetVelocity(address); }
        Vec3 GetServerPosition() const { return CoreAi::GetServerPosition(address); }
        Vec3 GetPathStart() const      { return CoreAi::GetPathStart(address); }
        Vec3 GetPathEnd() const        { return CoreAi::GetPathEnd(address); }
        Vec3 GetOrderPosition() const  { return CoreAi::GetOrderPosition(address); }

        Vec3 GetPreviousPosition() const {
            const Vec3 pathStart = CoreAi::GetPathStart(address);
            if (!pathStart.IsZero()) return pathStart;
            const Vec3 serverPos = CoreAi::GetServerPosition(address);
            if (!serverPos.IsZero()) return serverPos;
            return GetPosition();
        }

        int GetWaypointCount() const  { return CoreAi::GetWaypointCount(address); }
        int CopyWaypoints(Vec3* out, int maxOut) const { return CoreAi::CopyWaypoints(address, out, maxOut); }

        // ── Missile accessors ──

        uintptr_t GetMissileSpellData() const { return Globals::Read<uintptr_t>(address + Offset::MissileClient::SpellDataPtr); }
        CoreSpellCastInfo::MissileCastRef GetMissileCastInfo() const { return CoreSpellCastInfo::GetMissileCast(address); }
        int  GetMissileNetId() const { return Globals::Read<int>(address + Offset::MissileClient::MissileNetId); }
        int  GetMissileCasterNetId() const { return Globals::Read<int>(address + Offset::MissileClient::CasterNetId); }
        int  GetMissileTargetNetId() const { return Globals::Read<int>(address + Offset::MissileClient::TargetNetId); }
        Vec3 GetMissileStartPos() const    { return Globals::Read<Vec3>(address + Offset::MissileClient::StartPos); }
        Vec3 GetMissileEndPos() const      { return Globals::Read<Vec3>(address + Offset::MissileClient::EndPos); }
        Vec3 GetMissileCastEndPos() const  { return Globals::Read<Vec3>(address + Offset::MissileClient::CastEndPos); }
        bool ReadMissileSpellName(char* out, int maxOut) const {
            return IsValid() && Globals::ReadRuntimeStringField(address + Offset::MissileClient::SpellName, out, maxOut);
        }
        bool ReadMissileName(char* out, int maxOut) const {
            return IsValid() && Globals::ReadRuntimeStringField(address + Offset::MissileClient::MissileName, out, maxOut);
        }

        // ── Distance / team comparison ──

        float DistanceTo(const ObjectRef& other) const { return GetPosition().Distance2D(other.GetPosition()); }
        float DistanceTo(const Vec3& pos) const        { return GetPosition().Distance2D(pos); }

        bool IsLocalPlayer() const {
            return address == CoreRuntime::GetContext().localPlayer;
        }

        bool IsAlly() const {
            const auto local = CoreRuntime::GetContext().localPlayer;
            return Globals::IsValidPtr(local) && GetTeam() == ObjectRef{ local }.GetTeam();
        }

        bool IsEnemy() const {
            const auto local = CoreRuntime::GetContext().localPlayer;
            return Globals::IsValidPtr(local) && GetTeam() != 0 && GetTeam() != ObjectRef{ local }.GetTeam();
        }

        bool IsAllyTo(const ObjectRef& other) const {
            return IsValid() && other.IsValid() && GetTeam() == other.GetTeam();
        }

        bool IsEnemyTo(const ObjectRef& other) const {
            return IsValid() && other.IsValid() && GetTeam() != 0 && GetTeam() != other.GetTeam();
        }

        // ── Visibility / targetability ──

        bool IsVisible() const       { return Globals::Read<bool>(address + Offset::All::Visible); }
        bool IsTargetable() const    { return Globals::Read<bool>(address + Offset::AttackableUnit::IsTargetable); }
        bool IsInvulnerable() const  { return Globals::Read<bool>(address + Offset::All::IsInvulnerable); }
        bool IsVulnerable() const    { return !IsInvulnerable(); }

        // ── Liveness ──

        bool IsAlive() const {
            if (!IsValid()) return false;
            const auto fn = CoreRuntime::ResolveRva(Offset::ControlRuntime::IsAlive);
            if (!fn) return !Globals::Read<bool>(address + Offset::All::Dead);

            using fnIsAlive = bool(__fastcall*)(uintptr_t);
            __try {
                return reinterpret_cast<fnIsAlive>(fn)(address);
            } __except (1) {
                return !Globals::Read<bool>(address + Offset::All::Dead);
            }
        }

        bool IsDead() const {
            if (!IsValid()) return true;

            const auto fn = CoreRuntime::ResolveRva(Offset::ClassificationRuntime::IsDead);
            if (fn) {
                using fnIsDead = bool(__fastcall*)(uintptr_t);
                __try {
                    return reinterpret_cast<fnIsDead>(fn)(address);
                } __except (1) {}
            }

            if (Globals::Read<bool>(address + Offset::All::Dead)) return true;
            return !IsAlive();
        }

        // ── Classification (pure name-based via CoreClassification) ──

        CoreClassification::ObjectType GetObjectType() const { return CoreClassification::Classify(address); }

        bool IsHero() const       { return GetObjectType() == CoreClassification::ObjectType::Hero; }
        bool IsLaneMinion() const { return GetObjectType() == CoreClassification::ObjectType::LaneMinion; }
        bool IsTurret() const     { return GetObjectType() == CoreClassification::ObjectType::Turret; }
        bool IsPlant() const      { return GetObjectType() == CoreClassification::ObjectType::Plant; }
        bool IsPet() const        { return GetObjectType() == CoreClassification::ObjectType::Pet; }
        bool IsWard() const       { return GetObjectType() == CoreClassification::ObjectType::Ward; }
        bool IsInhibitor() const  { return GetObjectType() == CoreClassification::ObjectType::Inhibitor; }
        bool IsNexus() const      { return GetObjectType() == CoreClassification::ObjectType::Nexus; }
        bool IsStructure() const  { return CoreClassification::IsStructure(address); }

        bool IsMinion() const {
            const auto t = GetObjectType();
            return t == CoreClassification::ObjectType::LaneMinion ||
                   t == CoreClassification::ObjectType::JungleMonster ||
                   t == CoreClassification::ObjectType::JungleBig ||
                   t == CoreClassification::ObjectType::JungleEpic ||
                   t == CoreClassification::ObjectType::Scuttle ||
                   t == CoreClassification::ObjectType::Pet;
        }

        bool IsNeutral() const {
            const auto t = GetObjectType();
            return t == CoreClassification::ObjectType::JungleMonster ||
                   t == CoreClassification::ObjectType::JungleBig ||
                   t == CoreClassification::ObjectType::JungleEpic ||
                   t == CoreClassification::ObjectType::Scuttle ||
                   t == CoreClassification::ObjectType::Plant;
        }

        bool IsJungleMonster() const {
            const auto t = GetObjectType();
            return t == CoreClassification::ObjectType::JungleMonster ||
                   t == CoreClassification::ObjectType::JungleBig ||
                   t == CoreClassification::ObjectType::JungleEpic ||
                   t == CoreClassification::ObjectType::Scuttle;
        }

        // Memory-field jungle camp type (safer than calling ClassificationRuntime::IsBaron/IsDragon
        // game functions - no syscall, no hook detect surface).
        //   hero/camp + 0x4AB4 (uint8) : 0=Normal, 1=Baron, 2=Dragon
        // Returns false fast if address invalid.
        int JungleType() const {
            if (!IsValid()) return 0;
            return static_cast<int>(
                Globals::Read<uint8_t>(address + Offset::JungleTypeRuntime::TypeOffset));
        }
        bool IsBaron()  const { return JungleType() == Offset::JungleTypeRuntime::Baron; }
        bool IsDragon() const { return JungleType() == Offset::JungleTypeRuntime::Dragon; }

        // Vtable-based missile detection. Compares obj's vtable to the one
        // returned by the game's MissileClient type helper. The RVA is
        // hardcoded here — search for `kMissileTypeRVA` when updating for
        // a new build.
        bool IsMissile() const {
            const auto base = CoreRuntime::GetContext().moduleBase;
            if (!IsValid() || !base) return false;

            __try {
                auto* vtable = *reinterpret_cast<uintptr_t**>(address);
                using fnGetTypeID     = uintptr_t(__fastcall*)(uintptr_t);
                using fnGetMissileType = uintptr_t(*)();

                const auto getTypeID      = reinterpret_cast<fnGetTypeID>(vtable[1]);
                const auto getMissileType = reinterpret_cast<fnGetMissileType>(base + kMissileTypeRVA);
                return getTypeID(address) == getMissileType();
            } __except (1) {
                return false;
            }
        }

        bool ShouldIgnore() const       { return CoreClassification::ShouldIgnore(address); }
        bool IsAttackableBy(int myTeam) const { return CoreClassification::IsAttackable(address, myTeam); }

        bool IsMelee() const  { return GetAttackRange() <= 300.0f; }
        bool IsRanged() const { return !IsMelee(); }

        // ── NavGrid-derived predicates ──

        bool IsOnWall() const          { return CoreNavGrid::Get().IsWall(GetPosition()); }
        bool IsInBrush() const         { return CoreNavGrid::Get().IsBrush(GetPosition()); }
        bool IsWalkablePosition() const{ return CoreNavGrid::Get().IsWalkable(GetPosition()); }

        // ── Buffs ──

        bool HasBuff(const char* name) const            { return CoreBuffs::HasBuff(address, name); }
        bool HasBuffType(int type) const                { return CoreBuffs::HasBuffType(address, type); }
        int  GetBuffStacks(const char* name) const      { return CoreBuffs::GetBuffStacks(address, name); }
        float GetBuffRemainingTime(const char* name, float gameTime) const {
            return CoreBuffs::GetBuffRemainingTime(address, name, gameTime);
        }

        // ── Recall ──

        int  GetRecallState() const { return Globals::Read<int>(address + Offset::All::RecallState); }
        int  GetDeadFlagRaw() const { return Globals::Read<unsigned char>(address + Offset::All::Dead); }

        bool IsRecalling() const {
            if (!IsValid() || IsDead()) return false;

            // HasBuff("Recall") is active-filtered in CoreBuffs and is
            // suppressed as soon as activeSpellCast clears on cancel.
            if (CoreBuffs::HasBuff(address, "Recall")) return true;

            if (CoreBuffs::HasBuffRaw(address, "Recall") &&
                CoreBuffs::IsRecallChannelActive(address)) {
                return true;
            }

            // Truth source: the spellbook's `activeSpellCast` pointer.
            //
            // Why not the BuffManager?
            //   The recall buff entry stays in BuffManager with `stacks > 0`
            //   and `endTime` in the future even AFTER the player cancels
            //   the channel by issuing a move/attack order. The game only
            //   slides it out of the live entry range when the *original*
            //   recall timer would have completed. `HasActiveBuffContaining`
            //   therefore mis-reports the player as recalling for several
            //   seconds after a real cancel.
            //
            // Why the active spell cast?
            //   Pressing B starts a channel cast on `SpellSlot::Recall` (13).
            //   The spellbook's `activeSpellCast` pointer is overwritten with
            //   the new cast struct. When the channel ends — whether by
            //   completion (teleport home) OR by cancel (move / attack /
            //   damage interrupt) — the game CLEARS that pointer atomically
            //   on the same tick. So checking `activeSpellCast.IsValid() &&
            //   slot == 13` returns the correct value the same tick the
            //   cancel happens, regardless of what BuffManager is doing.
            //
            // The pointer chain is `hero+SpellRuntime::ActiveSpellCast`
            // which is a direct memory read (no syscall, no thread lock),
            // so it is just as cheap as the buff iteration it replaces.
            const auto cast = GetActiveSpellCast();
            if (!cast.IsValid()) return false;
            return cast.GetSlot() == CoreSpellBook::Slot_Recall;
        }

        bool IsWindingUp() const { return GetActiveSpellCast().IsValid(); }

        // ── Attack-range helpers ──

        float GetRealAttackRange(const ObjectRef& target) const {
            const float selfRadius = GetBoundingRadius();
            const float targetRadius = target.IsValid() ? target.GetBoundingRadius() : 0.0f;
            return GetAttackRange() + selfRadius + targetRadius;
        }

        bool IsInAutoAttackRange(const ObjectRef& target, float extraRange = 0.0f) const {
            if (!IsValid() || !target.IsValid()) return false;
            return DistanceTo(target) <= (GetRealAttackRange(target) + extraRange);
        }

        bool IsValidTarget(float maxRange = -1.0f,
                           bool requireVisible = true,
                           bool requireTargetable = true,
                           const Vec3& from = Vec3()) const {
            if (!IsValid() || IsDead()) return false;
            if (requireVisible && !IsVisible()) return false;
            if (requireTargetable && !IsTargetable()) return false;
            // NOTE: `All::IsInvulnerable` (0x5A0) reads garbage on current
            // builds, so invulnerability is NOT enforced here. Use a
            // buff-based check (Kayle R / Tryndamere R etc.) instead.
            if (maxRange >= 0.0f) {
                Vec3 origin = from;
                if (origin.IsZero()) {
                    const auto local = ObjectRef{ CoreRuntime::GetContext().localPlayer };
                    if (local.IsValid()) origin = local.GetPosition();
                }
                if (!origin.IsZero() && GetPosition().Distance2D(origin) > maxRange) {
                    return false;
                }
            }
            return true;
        }

        // ── Names ──

        bool ReadName(char* out, int maxOut) const {
            if (!IsValid()) {
                if (out && maxOut > 0) out[0] = 0;
                return false;
            }
            if (Globals::ReadGameString(address + Offset::All::Name, out, maxOut) && out[0]) {
                return true;
            }
            return Globals::ReadGameString(address + Offset::All::CharacterName, out, maxOut);
        }

        bool ReadCharacterName(char* out, int maxOut) const {
            if (!IsValid()) {
                if (out && maxOut > 0) out[0] = 0;
                return false;
            }
            return Globals::ReadGameString(address + Offset::All::CharacterName, out, maxOut);
        }

        // ── Animation ──

        uintptr_t GetAnimationComponentAddress() const {
            return IsValid() ? (address + Offset::AnimationLayout::Component) : 0;
        }

        uintptr_t GetAnimationResource() const {
            if (!IsValid()) return 0;
            const auto characterData = Globals::Read<uintptr_t>(address + Offset::AnimationLayout::CharacterData);
            if (!Globals::IsValidPtr(characterData)) return 0;
            const auto resource = Globals::Read<uintptr_t>(characterData + Offset::AnimationLayout::CharacterDataResource);
            return Globals::IsValidPtr(resource) ? resource : 0;
        }

        int GetAnimationVariantIndex() const {
            if (!IsValid()) return -1;
            return static_cast<int>(Globals::Read<short>(address + Offset::AnimationLayout::SkinIndex));
        }

        uintptr_t GetCurrentAnimationNameSlot() const {
            const auto resource = GetAnimationResource();
            if (!Globals::IsValidPtr(resource)) return 0;

            const int variantIndex = GetAnimationVariantIndex();
            if (variantIndex >= 0) {
                const int variantCount = Globals::Read<int>(resource + Offset::AnimationLayout::VariantEntryCount);
                const auto variantEntries = Globals::Read<uintptr_t>(resource + Offset::AnimationLayout::VariantEntries);
                if (variantCount > 0 && variantIndex < variantCount && Globals::IsValidPtr(variantEntries)) {
                    const auto entry = variantEntries +
                        (static_cast<uintptr_t>(variantIndex) * Offset::AnimationLayout::VariantEntryStride);
                    if (Globals::IsValidPtr(entry)) {
                        const auto variantSlot = entry + Offset::AnimationLayout::VariantNamePtr;
                        const auto variantName = Globals::Read<uintptr_t>(variantSlot);
                        if (Globals::IsValidPtr(variantName)) return variantSlot;
                    }
                }
            }

            const auto fallbackSlot = resource + Offset::AnimationLayout::FallbackNamePtr;
            const auto fallbackName = Globals::Read<uintptr_t>(fallbackSlot);
            return Globals::IsValidPtr(fallbackName) ? fallbackSlot : 0;
        }

        uintptr_t GetCurrentAnimationStateAddress() const {
            const auto resource = GetAnimationResource();
            if (!Globals::IsValidPtr(resource)) return 0;

            const int variantIndex = GetAnimationVariantIndex();
            if (variantIndex >= 0) {
                const int variantCount = Globals::Read<int>(resource + Offset::AnimationLayout::VariantEntryCount);
                const auto variantEntries = Globals::Read<uintptr_t>(resource + Offset::AnimationLayout::VariantEntries);
                if (variantCount > 0 && variantIndex < variantCount && Globals::IsValidPtr(variantEntries)) {
                    const auto entry = variantEntries +
                        (static_cast<uintptr_t>(variantIndex) * Offset::AnimationLayout::VariantEntryStride);
                    if (Globals::IsValidPtr(entry)) {
                        return entry + Offset::AnimationLayout::VariantState;
                    }
                }
            }
            return resource + Offset::AnimationLayout::FallbackState;
        }

        int GetAnimationQueueCount() const {
            if (!IsValid()) return 0;

            const auto begin = Globals::Read<uintptr_t>(address + Offset::AnimationLayout::Queue);
            const auto end   = Globals::Read<uintptr_t>(address + Offset::AnimationLayout::QueueEnd);
            if (!Globals::IsValidPtr(begin) || end < begin) return 0;

            const auto byteCount = end - begin;
            if ((byteCount % sizeof(int)) != 0 || byteCount > 0x1000) return 0;
            return static_cast<int>(byteCount / sizeof(int));
        }

        bool ReadCurrentAnimation(char* out, int maxOut) const {
            if (!out || maxOut <= 0) { if (out) out[0] = 0; return false; }
            out[0] = 0;

            const auto slot = GetCurrentAnimationNameSlot();
            if (!Globals::IsValidPtr(slot)) return false;

            const auto namePtr = Globals::Read<uintptr_t>(slot);
            if (!Globals::IsValidPtr(namePtr)) return false;

            return Globals::ReadGameString(namePtr, out, maxOut);
        }

        bool GetCurrentAnimation(char* out, int maxOut) const {
            return ReadCurrentAnimation(out, maxOut);
        }

        // ── Inventory ──

        bool HasItem(int slotIndex) const                { return CoreItem::HasItem(address, slotIndex); }
        uint32_t GetItemIdRaw(int slotIndex) const       { return CoreItem::GetItemIdRaw(address, slotIndex); }
        uintptr_t GetItemInfo(int slotIndex) const       { return CoreItem::GetItemInfo(address, slotIndex); }
        int  GetItemCount() const                        { return CoreItem::GetItemCount(address); }
        bool HasTrinket() const                          { return CoreItem::HasTrinket(address); }

        bool MatchesItemId(int slotIndex, uint32_t encryptedRef) const {
            return CoreItem::MatchesEncryptedId(address, slotIndex, encryptedRef);
        }

        int SnapshotItems(CoreItem::ItemSlot* out, int maxOut) const {
            return CoreItem::SnapshotItems(address, out, maxOut);
        }
    };

    // ─────────────────────────────────────────────────────────────────────
    // Manager enumeration
    // ─────────────────────────────────────────────────────────────────────

    inline ManagerView ReadManager(uintptr_t managerPtr) {
        ManagerView view = {};
        view.manager = managerPtr;
        if (!Globals::IsValidPtr(managerPtr)) return view;

        view.items = Globals::Read<uintptr_t>(managerPtr + Offset::ObjectManagerRuntime::ManagerListItems);
        view.count = Globals::Read<int>(managerPtr + Offset::ObjectManagerRuntime::ManagerListSize);
        return view;
    }

    inline int CopyManagerObjects(const ManagerView& view, uintptr_t* out, int maxOut) {
        if (!out || maxOut <= 0 || !view.IsValid() || view.count > maxOut) {
            return 0;
        }
        return Globals::ReadPtrArray(view.items, view.count, out, maxOut);
    }

    inline bool IsMissileTreeNil(uintptr_t node) {
        return !Globals::IsValidPtr(node) ||
               Globals::Read<unsigned char>(node + 0x19) != 0;
    }

    inline int CopyMissileTreeObjects(uintptr_t manager, uintptr_t* out, int maxOut) {
        if (!out || maxOut <= 0 || !Globals::IsValidPtr(manager)) {
            return 0;
        }

        const auto head = Globals::Read<uintptr_t>(manager + 0x8);
        const int size = Globals::Read<int>(manager + 0x10);
        if (!Globals::IsValidPtr(head) || size <= 0 || size > 4096) {
            return 0;
        }

        uintptr_t stack[256] = {};
        int stackSize = 0;
        int written = 0;
        int visited = 0;
        uintptr_t current = Globals::Read<uintptr_t>(head + 0x8);
        const int visitLimit = size * 2 + 32;

        while ((!IsMissileTreeNil(current) || stackSize > 0) && visited < visitLimit) {
            while (!IsMissileTreeNil(current)) {
                if (stackSize >= static_cast<int>(sizeof(stack) / sizeof(stack[0]))) {
                    return written;
                }
                stack[stackSize++] = current;
                current = Globals::Read<uintptr_t>(current);
            }

            if (stackSize <= 0) {
                break;
            }

            const auto node = stack[--stackSize];
            ++visited;

            const auto missile = Globals::Read<uintptr_t>(node + 0x28);
            if (Globals::IsValidPtr(missile) && written < maxOut) {
                out[written++] = missile;
            }

            current = Globals::Read<uintptr_t>(node + 0x10);
        }

        return written;
    }

    inline ManagerView GetHeroManagerView()    { return ReadManager(CoreRuntime::GetContext().heroManager); }
    inline ManagerView GetMinionManagerView()  { return ReadManager(CoreRuntime::GetContext().minionManager); }
    inline ManagerView GetTurretManagerView()  { return ReadManager(CoreRuntime::GetContext().turretManager); }
    inline ManagerView GetMissileManagerView() { return ReadManager(CoreRuntime::GetContext().missileManager); }

    inline int EnumerateHeroes(uintptr_t* out, int maxOut)    { return CopyManagerObjects(GetHeroManagerView(), out, maxOut); }
    inline int EnumerateMinions(uintptr_t* out, int maxOut)   { return CopyManagerObjects(GetMinionManagerView(), out, maxOut); }
    inline int EnumerateTurrets(uintptr_t* out, int maxOut)   { return CopyManagerObjects(GetTurretManagerView(), out, maxOut); }
    inline int EnumerateMissiles(uintptr_t* out, int maxOut)  { return CopyMissileTreeObjects(CoreRuntime::GetContext().missileManager, out, maxOut); }

    inline int FilterObjects(const uintptr_t* src, int srcCount,
                             uintptr_t* out, int maxOut,
                             bool(*predicate)(const ObjectRef&)) {
        if (!out || maxOut <= 0 || !src || srcCount <= 0 || !predicate) return 0;

        int written = 0;
        for (int i = 0; i < srcCount && written < maxOut; ++i) {
            const ObjectRef obj{ src[i] };
            if (!obj.IsValid()) continue;
            if (!predicate(obj)) continue;
            out[written++] = obj.address;
        }
        return written;
    }

    inline int EnumerateAllyHeroes(uintptr_t* out, int maxOut) {
        uintptr_t all[32] = {};
        const int count = EnumerateHeroes(all, static_cast<int>(sizeof(all) / sizeof(all[0])));
        return FilterObjects(all, count, out, maxOut, [](const ObjectRef& obj) { return obj.IsAlly(); });
    }

    inline int EnumerateEnemyHeroes(uintptr_t* out, int maxOut) {
        uintptr_t all[32] = {};
        const int count = EnumerateHeroes(all, static_cast<int>(sizeof(all) / sizeof(all[0])));
        return FilterObjects(all, count, out, maxOut, [](const ObjectRef& obj) { return obj.IsEnemy(); });
    }

    inline int EnumerateAllyMinions(uintptr_t* out, int maxOut) {
        uintptr_t all[512] = {};
        const int count = EnumerateMinions(all, static_cast<int>(sizeof(all) / sizeof(all[0])));
        return FilterObjects(all, count, out, maxOut, [](const ObjectRef& obj) {
            if (!obj.IsAlly()) return false;
            if (obj.IsLaneMinion()) return true;
            return obj.IsMinion() && !obj.IsJungleMonster() && !obj.IsPlant() && !obj.IsPet();
        });
    }

    inline int EnumerateEnemyMinions(uintptr_t* out, int maxOut) {
        uintptr_t all[512] = {};
        const int count = EnumerateMinions(all, static_cast<int>(sizeof(all) / sizeof(all[0])));
        return FilterObjects(all, count, out, maxOut, [](const ObjectRef& obj) {
            if (obj.IsAlly()) return false;
            if (obj.IsNeutral()) return false;
            if (obj.IsLaneMinion()) return true;
            return obj.IsMinion() && !obj.IsJungleMonster() && !obj.IsPlant() && !obj.IsPet();
        });
    }

    inline int EnumerateJungleMinions(uintptr_t* out, int maxOut) {
        uintptr_t all[512] = {};
        const int count = EnumerateMinions(all, static_cast<int>(sizeof(all) / sizeof(all[0])));
        return FilterObjects(all, count, out, maxOut, [](const ObjectRef& obj) {
            return obj.IsJungleMonster() && !obj.IsPlant();
        });
    }

    inline int EnumeratePlants(uintptr_t* out, int maxOut) {
        uintptr_t all[512] = {};
        const int count = EnumerateMinions(all, static_cast<int>(sizeof(all) / sizeof(all[0])));
        return FilterObjects(all, count, out, maxOut, [](const ObjectRef& obj) { return obj.IsPlant(); });
    }

    inline int EnumeratePets(uintptr_t* out, int maxOut) {
        uintptr_t all[512] = {};
        const int count = EnumerateMinions(all, static_cast<int>(sizeof(all) / sizeof(all[0])));
        return FilterObjects(all, count, out, maxOut, [](const ObjectRef& obj) { return obj.IsPet(); });
    }

    inline int EnumerateAllyTurrets(uintptr_t* out, int maxOut) {
        uintptr_t all[64] = {};
        const int count = EnumerateTurrets(all, static_cast<int>(sizeof(all) / sizeof(all[0])));
        return FilterObjects(all, count, out, maxOut, [](const ObjectRef& obj) { return obj.IsAlly(); });
    }

    inline int EnumerateEnemyTurrets(uintptr_t* out, int maxOut) {
        uintptr_t all[64] = {};
        const int count = EnumerateTurrets(all, static_cast<int>(sizeof(all) / sizeof(all[0])));
        return FilterObjects(all, count, out, maxOut, [](const ObjectRef& obj) { return obj.IsEnemy(); });
    }

    // ── Global iteration (uses GetFirstObject / GetNextObject when available) ──

    inline int EnumerateAllObjects(uintptr_t* out, int maxOut) {
        if (!out || maxOut <= 0) return 0;

        const auto& ctx = CoreRuntime::GetContext();
        if (!Globals::IsValidPtr(ctx.objectManager)) return 0;

        using fnGetFirst = uintptr_t(__cdecl*)(uintptr_t);
        using fnGetNext  = uintptr_t(__cdecl*)(uintptr_t, uintptr_t);

        const auto getFirst = reinterpret_cast<fnGetFirst>(CoreRuntime::ResolveRva(Offset::ObjectManagerRuntime::GetFirstObject));
        const auto getNext  = reinterpret_cast<fnGetNext>(CoreRuntime::ResolveRva(Offset::ObjectManagerRuntime::GetNextObject));

        if (getFirst && getNext) {
            int count = 0;
            __try {
                CoreBypass::MainloopCheck();
                uintptr_t obj = getFirst(ctx.objectManager);
                while (Globals::IsValidPtr(obj) && count < maxOut) {
                    out[count++] = obj;
                    obj = getNext(ctx.objectManager, obj);
                }
            } __except (1) {
                return count;
            }

            if (count > 0) return count;
        }

        // Fallback: read the flat manager list directly.
        const auto list  = Globals::Read<uintptr_t>(ctx.objectManager + Offset::ObjectManagerRuntime::ManagerListItems);
        const auto count = Globals::Read<int>(ctx.objectManager + Offset::ObjectManagerRuntime::ManagerListSize);
        if (!Globals::IsValidPtr(list) || count <= 0 || count > maxOut) return 0;
        return Globals::ReadPtrArray(list, count, out, maxOut);
    }

    // ── Common lookups ──

    inline ObjectRef GetLocalPlayer()       { return { CoreRuntime::GetContext().localPlayer }; }
    inline ObjectRef GetUnderMouseObject()  { return { CoreRuntime::GetContext().underMouseObject }; }

    inline ObjectRef FindByNetId(int netId) {
        if (netId <= 0) return {};

        uintptr_t objects[4096] = {};
        const int count = EnumerateAllObjects(objects, static_cast<int>(sizeof(objects) / sizeof(objects[0])));
        for (int i = 0; i < count; ++i) {
            ObjectRef obj{ objects[i] };
            if (obj.IsValid() && obj.GetNetId() == netId) return obj;
        }
        return {};
    }

    inline ObjectRef FindByIndex(int index) {
        if (index <= 0) return {};

        uintptr_t objects[4096] = {};
        const int count = EnumerateAllObjects(objects, static_cast<int>(sizeof(objects) / sizeof(objects[0])));
        for (int i = 0; i < count; ++i) {
            ObjectRef obj{ objects[i] };
            if (obj.IsValid() && obj.GetIndex() == index) return obj;
        }
        return {};
    }

    // ── Range-based counters ──

    inline int CountInRange(const uintptr_t* objects, int count, float range, const Vec3& from) {
        if (!objects || count <= 0) return 0;

        const ObjectRef local = GetLocalPlayer();
        const Vec3 origin = from.IsZero() && local.IsValid() ? local.GetPosition() : from;
        if (origin.IsZero()) return 0;

        int hits = 0;
        for (int i = 0; i < count; ++i) {
            const ObjectRef obj{ objects[i] };
            if (!obj.IsValid() || obj.IsDead()) continue;
            if (obj.GetPosition().Distance2D(origin) <= range) ++hits;
        }
        return hits;
    }

    inline int CountEnemyHeroesInRange(float range, const Vec3& from = Vec3()) {
        uintptr_t heroes[32] = {};
        const int count = EnumerateEnemyHeroes(heroes, static_cast<int>(sizeof(heroes) / sizeof(heroes[0])));
        return CountInRange(heroes, count, range, from);
    }

    inline int CountAllyHeroesInRange(float range, const Vec3& from = Vec3()) {
        uintptr_t heroes[32] = {};
        const int count = EnumerateAllyHeroes(heroes, static_cast<int>(sizeof(heroes) / sizeof(heroes[0])));
        return CountInRange(heroes, count, range, from);
    }

    inline int CountEnemyMinionsInRange(float range, const Vec3& from = Vec3()) {
        uintptr_t minions[512] = {};
        const int count = EnumerateEnemyMinions(minions, static_cast<int>(sizeof(minions) / sizeof(minions[0])));
        return CountInRange(minions, count, range, from);
    }

    inline int CountAllyMinionsInRange(float range, const Vec3& from = Vec3()) {
        uintptr_t minions[512] = {};
        const int count = EnumerateAllyMinions(minions, static_cast<int>(sizeof(minions) / sizeof(minions[0])));
        return CountInRange(minions, count, range, from);
    }

} // namespace CoreObjects
