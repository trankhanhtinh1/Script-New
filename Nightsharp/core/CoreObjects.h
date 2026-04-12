#pragma once

#include "Globals.h"
#include "Offsets.h"
#include "CoreAi.h"
#include "CoreBuffs.h"
#include "CoreBypass.h"
#include "CoreNavGrid.h"
#include "CoreRuntime.h"
#include "CoreSpellBook.h"
#include "CoreSpellCastInfo.h"
#include "RuntimeAPI.h"
#include "Vector.h"
#include <cstdint>

namespace CoreObjects {

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

    struct ObjectRef {
        uintptr_t address = 0;

        bool IsValid() const {
            return Globals::IsValidPtr(address);
        }

        int GetNetId() const {
            return Globals::Read<int>(address + Offset::GameObject::NetId);
        }

        int GetIndex() const {
            return Globals::Read<int>(address + Offset::GameObject::Index);
        }

        int GetTeam() const {
            return Globals::Read<unsigned char>(address + Offset::GameObject::TeamAlt);
        }

        Vec3 GetPosition() const {
            return Globals::Read<Vec3>(address + Offset::GameObject::Position);
        }

        Vec3 GetDirection() const {
            return Globals::Read<Vec3>(address + Offset::GameObject::Direction);
        }

        float GetHealth() const {
            return Globals::Read<float>(address + Offset::Health::HP);
        }

        float GetMaxHealth() const {
            return Globals::Read<float>(address + Offset::Health::MaxHP);
        }

        float GetMana() const {
            return Globals::Read<float>(address + Offset::Mana::MP);
        }

        float GetMaxMana() const {
            return Globals::Read<float>(address + Offset::Mana::MaxMP);
        }

        float GetMoveSpeed() const {
            return Globals::Read<float>(address + Offset::HeroStats::MoveSpeed);
        }

        float GetHPRegenRate() const {
            return Globals::Read<float>(address + Offset::HeroStats::HPRegenRate);
        }

        float GetBaseHPRegenRate() const {
            return Globals::Read<float>(address + Offset::HeroStats::BaseHPRegenRate);
        }

        float GetBoundingRadius() const {
            using fnGetBoundingRadius = float(__fastcall*)(uintptr_t);
            const auto fn = CoreRuntime::GetContext().moduleBase
                ? (CoreRuntime::GetContext().moduleBase + Offset::Function::GetBoundingRadius)
                : 0;
            if (!fn) {
                return Globals::Read<float>(address + Offset::GameObject::Radius);
            }

            __try {
                const float radius = reinterpret_cast<fnGetBoundingRadius>(fn)(address);
                if (radius > 0.0f && radius < 500.0f) {
                    return radius;
                }
            }
            __except (1) {
            }

            return Globals::Read<float>(address + Offset::GameObject::Radius);
        }

        float GetAttackRange() const {
            return Globals::Read<float>(address + Offset::HeroStats::AttackRange);
        }

        float GetBaseAD() const {
            return Globals::Read<float>(address + Offset::HeroStats::BaseAttackDamage);
        }

        float GetBonusAD() const {
            return Globals::Read<float>(address + Offset::HeroStats::FlatPhysicalDmgMod);
        }

        float GetTotalAD() const {
            return GetBaseAD() + GetBonusAD();
        }

        float GetAbilityPower() const {
            return Globals::Read<float>(address + Offset::HeroStats::BaseAbilityDamage);
        }

        float GetArmor() const {
            return Globals::Read<float>(address + Offset::HeroStats::Armor);
        }

        float GetBonusArmor() const {
            return Globals::Read<float>(address + Offset::HeroStats::BonusArmor);
        }

        float GetSpellBlock() const {
            return Globals::Read<float>(address + Offset::HeroStats::SpellBlock);
        }

        float GetBonusSpellBlock() const {
            return Globals::Read<float>(address + Offset::HeroStats::BonusSpellBlock);
        }

        float GetAllShield() const {
            return Globals::Read<float>(address + Offset::Health::AllShield);
        }

        float GetPhysicalShield() const {
            return Globals::Read<float>(address + Offset::Health::PhysicalShield);
        }

        float GetMagicalShield() const {
            return Globals::Read<float>(address + Offset::Health::MagicalShield);
        }

        float GetTotalShield() const {
            return GetAllShield() + GetPhysicalShield() + GetMagicalShield();
        }

        float GetEffectiveHealth() const {
            return GetHealth() + GetTotalShield();
        }

        float GetCrit() const {
            return Globals::Read<float>(address + Offset::HeroStats::Crit);
        }

        float GetCritMultiplier() const {
            return Globals::Read<float>(address + Offset::HeroStats::CritDamageMultiplier);
        }

        float GetAttackSpeedMod() const {
            return Globals::Read<float>(address + Offset::HeroStats::AttackSpeedMod);
        }

        float GetPercentAttackSpeedMod() const {
            return Globals::Read<float>(address + Offset::HeroStats::PercentAttackSpeedMod);
        }

        float GetFlatBaseAttackSpeedMod() const {
            return Globals::Read<float>(address + Offset::HeroStats::FlatBaseAttackSpeedMod);
        }

        float GetAbilityHaste() const {
            return Globals::Read<float>(address + Offset::HeroStats::AbilityHaste);
        }

        float GetArmorPenFlat() const {
            return Globals::Read<float>(address + Offset::HeroStats::FlatArmorPen);
        }

        float GetLethality() const {
            return Globals::Read<float>(address + Offset::HeroStats::PhysicalLethality);
        }

        float GetArmorPenPercent() const {
            return Globals::Read<float>(address + Offset::HeroStats::PercentArmorPen);
        }

        float GetBonusArmorPenPercent() const {
            return Globals::Read<float>(address + Offset::HeroStats::PercentBonusArmorPen);
        }

        float GetMagicPenFlat() const {
            return Globals::Read<float>(address + Offset::HeroStats::FlatMagicPen);
        }

        float GetMagicPenPercent() const {
            return Globals::Read<float>(address + Offset::HeroStats::PercentMagicPen);
        }

        float GetBonusMagicPenPercent() const {
            return Globals::Read<float>(address + Offset::HeroStats::PercentBonusMagicPen);
        }

        float GetLifeSteal() const {
            return Globals::Read<float>(address + Offset::HeroStats::PercentLifeSteal);
        }

        float GetSpellVamp() const {
            return Globals::Read<float>(address + Offset::HeroStats::PercentSpellVamp);
        }

        float GetOmnivamp() const {
            return Globals::Read<float>(address + Offset::HeroStats::PercentOmnivamp);
        }

        float GetHealthPercent() const {
            const float maxHealth = GetMaxHealth();
            return maxHealth > 0.0f ? (GetHealth() / maxHealth) * 100.0f : 0.0f;
        }

        float GetManaPercent() const {
            const float maxMana = GetMaxMana();
            return maxMana > 0.0f ? (GetMana() / maxMana) * 100.0f : 0.0f;
        }

        int GetActionState() const {
            return Globals::Read<int>(address + Offset::ActionState::State1);
        }

        int GetActionState2() const {
            return Globals::Read<int>(address + Offset::ActionState::State2);
        }

        bool CanAttack() const {
            return (GetActionState() & Offset::ActionState::CanAttack) != 0;
        }

        bool CanMove() const {
            return (GetActionState() & Offset::ActionState::CanMove) != 0;
        }

        bool CanCast() const {
            return (GetActionState() & Offset::ActionState::CanCast) != 0;
        }

        bool IsImmobile() const {
            return (GetActionState() & Offset::ActionState::ImmobileMask) != 0;
        }

        bool HasForcedMove() const {
            return (GetActionState() & Offset::ActionState::ForcedMoveMask) != 0;
        }

        bool IsGrounded() const {
            return (GetActionState() & Offset::ActionState::NoDashMask) != 0;
        }

        bool CanDash() const {
            return !IsGrounded();
        }

        bool IsCrowdControlled() const {
            return IsImmobile() || HasForcedMove();
        }

        int GetLevel() const {
            return Globals::Read<int>(address + Offset::Hero::LevelRef);
        }

        int GetLevelUpPoints() const {
            return Globals::Read<int>(address + Offset::Hero::LevelUpPoints);
        }

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

        // ── Animation name resolution ──
        // Chain: obj+CharacterData → +CharacterDataResource → variant or fallback
        bool ReadCurrentAnimation(char* out, int maxOut) const {
            if (!out || maxOut <= 0) { out[0] = 0; return false; }
            out[0] = 0;

            const auto slot = GetCurrentAnimationNameSlot();
            if (!Globals::IsValidPtr(slot)) {
                return false;
            }

            const auto namePtr = Globals::Read<uintptr_t>(slot);
            if (!Globals::IsValidPtr(namePtr)) {
                return false;
            }

            return Globals::ReadGameString(namePtr, out, maxOut);
        }

        uintptr_t GetBuffManager() const {
            return CoreBuffs::GetBuffManager(address);
        }

        CoreAi::ManagerRef GetAiManager() const {
            return CoreAi::Get(address);
        }

        bool HasPath() const {
            return CoreAi::HasPath(address);
        }

        bool IsMovingOnPath() const {
            return CoreAi::IsMoving(address);
        }

        bool IsDashingOnPath() const {
            return CoreAi::IsDashing(address);
        }

        int GetCurrentPathSegment() const {
            return CoreAi::GetCurrentSegment(address);
        }

        Vec3 GetVelocity() const {
            return CoreAi::GetVelocity(address);
        }

        Vec3 GetServerPosition() const {
            return CoreAi::GetServerPosition(address);
        }

        Vec3 GetPreviousPosition() const {
            const Vec3 pathStart = CoreAi::GetPathStart(address);
            if (!pathStart.IsZero()) {
                return pathStart;
            }

            const Vec3 serverPos = CoreAi::GetServerPosition(address);
            if (!serverPos.IsZero()) {
                return serverPos;
            }

            return GetPosition();
        }

        Vec3 GetPathStart() const {
            return CoreAi::GetPathStart(address);
        }

        Vec3 GetPathEnd() const {
            return CoreAi::GetPathEnd(address);
        }

        Vec3 GetOrderPosition() const {
            return CoreAi::GetOrderPosition(address);
        }

        int GetWaypointCount() const {
            return CoreAi::GetWaypointCount(address);
        }

        int CopyWaypoints(Vec3* out, int maxOut) const {
            return CoreAi::CopyWaypoints(address, out, maxOut);
        }

        uintptr_t GetMissileSpellData() const {
            return Globals::Read<uintptr_t>(address + Offset::Missile::SpellDataPtr);
        }

        CoreSpellCastInfo::CastRef GetMissileCastInfo() const {
            return CoreSpellCastInfo::GetMissileCast(address);
        }

        int GetMissileCasterNetId() const {
            return Globals::Read<int>(address + Offset::Missile::CasterNetId);
        }

        int GetMissileTargetNetId() const {
            return Globals::Read<int>(address + Offset::Missile::TargetNetId);
        }

        Vec3 GetMissileStartPos() const {
            return Globals::Read<Vec3>(address + Offset::Missile::StartPos);
        }

        Vec3 GetMissileEndPos() const {
            return Globals::Read<Vec3>(address + Offset::Missile::EndPos);
        }

        Vec3 GetMissileCastEndPos() const {
            return Globals::Read<Vec3>(address + Offset::Missile::CastEndPos);
        }

        float DistanceTo(const ObjectRef& other) const {
            return GetPosition().Distance2D(other.GetPosition());
        }

        float DistanceTo(const Vec3& pos) const {
            return GetPosition().Distance2D(pos);
        }

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

        bool IsVisible() const {
            return Globals::Read<bool>(address + Offset::GameObject::Visible);
        }

        bool IsTargetable() const {
            return Globals::Read<bool>(address + Offset::Targetable::IsTargetable);
        }

        bool IsInvulnerable() const {
            return Globals::Read<bool>(address + Offset::GameObject::IsInvulnerable);
        }

        bool IsVulnerable() const {
            return !IsInvulnerable();
        }

        bool IsAlive() const {
            return RuntimeAPI::IsAlive(address);
        }

        bool IsDead() const {
            if (!IsValid()) {
                return true;
            }

            using fnIsDead = bool(__fastcall*)(uintptr_t);
            const auto fn = CoreRuntime::GetContext().moduleBase
                ? (CoreRuntime::GetContext().moduleBase + Offset::Function::IsDead)
                : 0;
            if (fn) {
                __try {
                    return reinterpret_cast<fnIsDead>(fn)(address);
                }
                __except (1) {
                }
            }

            if (Globals::Read<bool>(address + Offset::GameObject::Dead)) {
                return true;
            }

            return !IsAlive();
        }

        // ====================================================================
        // Classification — delegate to RuntimeAPI directly
        // These thin wrappers exist only for use by CoreObjects enumeration.
        // SDK layer calls RuntimeAPI:: directly (matching old NightSharp).
        // ====================================================================
        bool IsHero() const { return RuntimeAPI::IsHero(address); }
        bool IsMinion() const { return RuntimeAPI::IsMinion(address); }
        bool IsLaneMinion() const { return RuntimeAPI::IsLaneMinion(address); }
        bool IsTurret() const { return RuntimeAPI::IsTurret(address); }
        bool IsPlant() const { return RuntimeAPI::IsPlant(address); }
        bool IsPet() const { return RuntimeAPI::IsPet(address); }
        bool IsNeutral() const { return RuntimeAPI::IsNeutral(address); }
        bool IsJungleMonster() const { return RuntimeAPI::IsJungleMonster(address); }
        bool IsMissile() const { return RuntimeAPI::IsMissile(address); }

        bool IsMelee() const {
            return GetAttackRange() <= 300.0f;
        }

        bool IsRanged() const {
            return !IsMelee();
        }

        bool IsOnWall() const {
            return CoreNavGrid::Get().IsWall(GetPosition());
        }

        bool IsInBrush() const {
            return CoreNavGrid::Get().IsBrush(GetPosition());
        }

        bool IsWalkablePosition() const {
            return CoreNavGrid::Get().IsWalkable(GetPosition());
        }

        bool HasBuff(const char* name) const {
            return CoreBuffs::HasBuff(address, name);
        }

        bool HasBuffType(int type) const {
            return CoreBuffs::HasBuffType(address, type);
        }

        int GetBuffStacks(const char* name) const {
            return CoreBuffs::GetBuffStacks(address, name);
        }

        float GetBuffRemainingTime(const char* name, float gameTime) const {
            return CoreBuffs::GetBuffRemainingTime(address, name, gameTime);
        }

        int GetRecallState() const {
            return Globals::Read<int>(address + Offset::GameObject::RecallState);
        }

        int GetDeadFlagRaw() const {
            return Globals::Read<unsigned char>(address + Offset::GameObject::Dead);
        }

        bool IsRecalling() const {
            if (!IsValid()) {
                return false;
            }

            // Check recall buff from BuffManager (no type filter — type may vary across versions)
            const bool hasRecallBuff = CoreBuffs::HasBuffContaining(address, "recall");
            if (!hasRecallBuff) {
                return false;
            }

            // ── Cancel detection ──
            // In newer client versions, the recall buff lingers briefly after
            // cancellation. If the player is moving or dashing, the recall was
            // cancelled and we should NOT block combat actions.
            if (IsMovingOnPath() || IsDashingOnPath()) {
                return false;
            }

            // Player has recall buff AND is standing still → genuinely recalling
            return true;
        }

        bool IsWindingUp() const {
            return GetActiveSpellCast().IsValid();
        }

        float GetRealAttackRange(const ObjectRef& target) const {
            const float selfRadius = GetBoundingRadius();
            const float targetRadius = target.IsValid() ? target.GetBoundingRadius() : 0.0f;
            return GetAttackRange() + selfRadius + targetRadius;
        }

        bool IsInAutoAttackRange(const ObjectRef& target, float extraRange = 0.0f) const {
            if (!IsValid() || !target.IsValid()) {
                return false;
            }
            return DistanceTo(target) <= (GetRealAttackRange(target) + extraRange);
        }

        bool IsValidTarget(float maxRange = -1.0f, bool requireVisible = true, bool requireTargetable = true, const Vec3& from = Vec3()) const {
            if (!IsValid() || IsDead()) {
                return false;
            }
            if (requireVisible && !IsVisible()) {
                return false;
            }
            if (requireTargetable && !IsTargetable()) {
                return false;
            }
            // NOTE: IsInvulnerable() offset (0x5A0) is UNVALIDATED and reads garbage
            // bytes (180, 12, 248 etc instead of 0/1). This was causing ALL targets
            // to be rejected. Invulnerability should be checked via buff-based logic
            // (Kayle R, Tryndamere R, etc.) instead of this raw memory read.
            // TODO: Find the correct IsInvulnerable offset or implement buff-based check
            // if (IsInvulnerable()) {
            //     return false;
            // }
            if (maxRange >= 0.0f) {
                Vec3 origin = from;
                if (origin.IsZero()) {
                    const auto local = ObjectRef{ CoreRuntime::GetContext().localPlayer };
                    if (local.IsValid()) {
                        origin = local.GetPosition();
                    }
                }
                if (!origin.IsZero() && GetPosition().Distance2D(origin) > maxRange) {
                    return false;
                }
            }
            return true;
        }

        bool ReadName(char* out, int maxOut) const {
            if (!IsValid()) {
                if (out && maxOut > 0) out[0] = 0;
                return false;
            }
            // Try DisplayName (0x70 = RiotId) first, fall back to Name (0x58)
            if (Globals::ReadGameString(address + Offset::All::Name, out, maxOut) && out[0])
                return true;
            return Globals::ReadGameString(address + Offset::GameObject::Name, out, maxOut);
        }

        bool ReadCharacterName(char* out, int maxOut) const {
            if (!IsValid()) {
                if (out && maxOut > 0) out[0] = 0;
                return false;
            }
            return Globals::ReadGameString(address + Offset::GameObject::CharacterName, out, maxOut);
        }

        uintptr_t GetAnimationComponentAddress() const {
            return IsValid() ? (address + Offset::Animation::Component) : 0;
        }

        uintptr_t GetAnimationResource() const {
            if (!IsValid()) {
                return 0;
            }

            const auto characterData = Globals::Read<uintptr_t>(address + Offset::Animation::CharacterData);
            if (!Globals::IsValidPtr(characterData)) {
                return 0;
            }

            const auto resource = Globals::Read<uintptr_t>(characterData + Offset::Animation::CharacterDataResource);
            return Globals::IsValidPtr(resource) ? resource : 0;
        }

        int GetAnimationVariantIndex() const {
            if (!IsValid()) {
                return -1;
            }

            return static_cast<int>(Globals::Read<short>(address + Offset::Animation::SkinIndex));
        }

        uintptr_t GetCurrentAnimationNameSlot() const {
            const auto resource = GetAnimationResource();
            if (!Globals::IsValidPtr(resource)) {
                return 0;
            }

            const int variantIndex = GetAnimationVariantIndex();
            if (variantIndex >= 0) {
                const int variantCount = Globals::Read<int>(resource + Offset::Animation::VariantEntryCount);
                const auto variantEntries = Globals::Read<uintptr_t>(resource + Offset::Animation::VariantEntries);
                if (variantCount > 0 &&
                    variantIndex < variantCount &&
                    Globals::IsValidPtr(variantEntries)) {
                    const auto entry = variantEntries +
                        (static_cast<uintptr_t>(variantIndex) * Offset::Animation::VariantEntryStride);
                    if (Globals::IsValidPtr(entry)) {
                        const auto variantSlot = entry + Offset::Animation::VariantNamePtr;
                        const auto variantName = Globals::Read<uintptr_t>(variantSlot);
                        if (Globals::IsValidPtr(variantName)) {
                            return variantSlot;
                        }
                    }
                }
            }

            const auto fallbackSlot = resource + Offset::Animation::CurrentAnimation;
            const auto fallbackName = Globals::Read<uintptr_t>(fallbackSlot);
            return Globals::IsValidPtr(fallbackName) ? fallbackSlot : 0;
        }

        uintptr_t GetCurrentAnimationStateAddress() const {
            const auto resource = GetAnimationResource();
            if (!Globals::IsValidPtr(resource)) {
                return 0;
            }

            const int variantIndex = GetAnimationVariantIndex();
            if (variantIndex >= 0) {
                const int variantCount = Globals::Read<int>(resource + Offset::Animation::VariantEntryCount);
                const auto variantEntries = Globals::Read<uintptr_t>(resource + Offset::Animation::VariantEntries);
                if (variantCount > 0 &&
                    variantIndex < variantCount &&
                    Globals::IsValidPtr(variantEntries)) {
                    const auto entry = variantEntries +
                        (static_cast<uintptr_t>(variantIndex) * Offset::Animation::VariantEntryStride);
                    if (Globals::IsValidPtr(entry)) {
                        return entry + Offset::Animation::VariantState;
                    }
                }
            }

            return resource + Offset::Animation::CurrentAnimationState;
        }

        int GetAnimationQueueCount() const {
            if (!IsValid()) {
                return 0;
            }

            const auto begin = Globals::Read<uintptr_t>(address + Offset::Animation::Queue);
            const auto end = Globals::Read<uintptr_t>(address + Offset::Animation::QueueEnd);
            if (!Globals::IsValidPtr(begin) || end < begin) {
                return 0;
            }

            const auto byteCount = end - begin;
            if ((byteCount % sizeof(int)) != 0 || byteCount > 0x1000) {
                return 0;
            }

            return static_cast<int>(byteCount / sizeof(int));
        }

        bool GetCurrentAnimation(char* out, int maxOut) const {
            return ReadCurrentAnimation(out, maxOut);
        }
    };

    inline ManagerView ReadManager(uintptr_t managerPtr) {
        ManagerView view = {};
        view.manager = managerPtr;
        if (!Globals::IsValidPtr(managerPtr)) {
            return view;
        }

        view.items = Globals::Read<uintptr_t>(managerPtr + Offset::ManagerList::Items);
        view.count = Globals::Read<int>(managerPtr + Offset::ManagerList::Size);
        return view;
    }

    inline int CopyManagerObjects(const ManagerView& view, uintptr_t* out, int maxOut) {
        if (!out || maxOut <= 0 || !view.IsValid() || view.count > maxOut) {
            return 0;
        }
        return Globals::ReadPtrArray(view.items, view.count, out, maxOut);
    }

    inline ManagerView GetHeroManagerView() {
        return ReadManager(CoreRuntime::GetContext().heroManager);
    }

    inline ManagerView GetMinionManagerView() {
        return ReadManager(CoreRuntime::GetContext().minionManager);
    }

    inline ManagerView GetTurretManagerView() {
        return ReadManager(CoreRuntime::GetContext().turretManager);
    }

    inline ManagerView GetMissileManagerView() {
        return ReadManager(CoreRuntime::GetContext().missileManager);
    }

    inline int EnumerateHeroes(uintptr_t* out, int maxOut) {
        return CopyManagerObjects(GetHeroManagerView(), out, maxOut);
    }

    inline int EnumerateMinions(uintptr_t* out, int maxOut) {
        return CopyManagerObjects(GetMinionManagerView(), out, maxOut);
    }

    inline int EnumerateTurrets(uintptr_t* out, int maxOut) {
        return CopyManagerObjects(GetTurretManagerView(), out, maxOut);
    }

    inline int EnumerateMissiles(uintptr_t* out, int maxOut) {
        return CopyManagerObjects(GetMissileManagerView(), out, maxOut);
    }

    inline int FilterObjects(const uintptr_t* src, int srcCount, uintptr_t* out, int maxOut, bool(*predicate)(const ObjectRef&)) {
        if (!out || maxOut <= 0 || !src || srcCount <= 0 || !predicate) {
            return 0;
        }

        int written = 0;
        for (int i = 0; i < srcCount && written < maxOut; ++i) {
            const ObjectRef obj{ src[i] };
            if (!obj.IsValid()) {
                continue;
            }
            if (!predicate(obj)) {
                continue;
            }
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

    // ================================================================
    // Object Enumeration — Classification via RuntimeAPI
    // Follows old NightSharp logic: separate lists for lane minions,
    // jungle monsters, plants, and pets.
    // ================================================================

    inline int EnumerateAllyMinions(uintptr_t* out, int maxOut) {
        uintptr_t all[512] = {};
        const int count = EnumerateMinions(all, static_cast<int>(sizeof(all) / sizeof(all[0])));
        return FilterObjects(all, count, out, maxOut, [](const ObjectRef& obj) {
            // Lane minions only — RuntimeAPI::IsLaneMinion checks MinionClass byte
            // This automatically excludes plants, pets, jungle monsters
            if (!obj.IsAlly()) return false;
            if (obj.IsLaneMinion()) return true;
            // Fallback: IsMinion but NOT any special type
            return obj.IsMinion() && !obj.IsJungleMonster() && !obj.IsPlant() && !obj.IsPet();
        });
    }

    inline int EnumerateEnemyMinions(uintptr_t* out, int maxOut) {
        uintptr_t all[512] = {};
        const int count = EnumerateMinions(all, static_cast<int>(sizeof(all) / sizeof(all[0])));
        return FilterObjects(all, count, out, maxOut, [](const ObjectRef& obj) {
            // Must not be ally team AND must not be neutral (team 300)
            if (obj.IsAlly()) return false;
            if (obj.IsNeutral()) return false; // Plants, jungle monsters are neutral
            // Lane minions only
            if (obj.IsLaneMinion()) return true;
            // Fallback: IsMinion but NOT any special type
            return obj.IsMinion() && !obj.IsJungleMonster() && !obj.IsPlant() && !obj.IsPet();
        });
    }

    inline int EnumerateJungleMinions(uintptr_t* out, int maxOut) {
        uintptr_t all[512] = {};
        const int count = EnumerateMinions(all, static_cast<int>(sizeof(all) / sizeof(all[0])));
        return FilterObjects(all, count, out, maxOut, [](const ObjectRef& obj) {
            // Jungle monsters — explicitly exclude plants
            return obj.IsJungleMonster() && !obj.IsPlant();
        });
    }

    inline int EnumeratePlants(uintptr_t* out, int maxOut) {
        uintptr_t all[512] = {};
        const int count = EnumerateMinions(all, static_cast<int>(sizeof(all) / sizeof(all[0])));
        return FilterObjects(all, count, out, maxOut, [](const ObjectRef& obj) {
            return obj.IsPlant();
        });
    }

    inline int EnumeratePets(uintptr_t* out, int maxOut) {
        uintptr_t all[512] = {};
        const int count = EnumerateMinions(all, static_cast<int>(sizeof(all) / sizeof(all[0])));
        return FilterObjects(all, count, out, maxOut, [](const ObjectRef& obj) {
            return obj.IsPet();
        });
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

    inline int EnumerateAllObjects(uintptr_t* out, int maxOut) {
        if (!out || maxOut <= 0) {
            return 0;
        }

        const auto& ctx = CoreRuntime::GetContext();
        if (!Globals::IsValidPtr(ctx.objectManager)) {
            return 0;
        }

        using fnGetFirst = uintptr_t(__cdecl*)(uintptr_t);
        using fnGetNext = uintptr_t(__cdecl*)(uintptr_t, uintptr_t);

        const auto getFirst = reinterpret_cast<fnGetFirst>(CoreRuntime::ResolveRva(Offset::Function::GetFirstObject));
        const auto getNext = reinterpret_cast<fnGetNext>(CoreRuntime::ResolveRva(Offset::Function::GetNextObject));

        if (getFirst && getNext) {
            int count = 0;
            __try {
                CoreBypass::MainloopCheck();
                uintptr_t obj = getFirst(ctx.objectManager);
                while (Globals::IsValidPtr(obj) && count < maxOut) {
                    out[count++] = obj;
                    obj = getNext(ctx.objectManager, obj);
                }
            }
            __except (1) {
                return count;
            }

            if (count > 0) {
                return count;
            }
        }

        const auto list = Globals::Read<uintptr_t>(ctx.objectManager + Offset::ManagerList::Items);
        const auto count = Globals::Read<int>(ctx.objectManager + Offset::ManagerList::Size);
        if (!Globals::IsValidPtr(list) || count <= 0 || count > maxOut) {
            return 0;
        }
        return Globals::ReadPtrArray(list, count, out, maxOut);
    }

    inline ObjectRef GetLocalPlayer() {
        return { CoreRuntime::GetContext().localPlayer };
    }

    inline ObjectRef GetUnderMouseObject() {
        return { CoreRuntime::GetContext().underMouseObject };
    }

    inline ObjectRef FindByNetId(int netId) {
        if (netId <= 0) {
            return {};
        }

        uintptr_t objects[4096] = {};
        const int count = EnumerateAllObjects(objects, static_cast<int>(sizeof(objects) / sizeof(objects[0])));
        for (int i = 0; i < count; ++i) {
            ObjectRef obj{ objects[i] };
            if (obj.IsValid() && obj.GetNetId() == netId) {
                return obj;
            }
        }
        return {};
    }

    inline ObjectRef FindByIndex(int index) {
        if (index <= 0) {
            return {};
        }

        uintptr_t objects[4096] = {};
        const int count = EnumerateAllObjects(objects, static_cast<int>(sizeof(objects) / sizeof(objects[0])));
        for (int i = 0; i < count; ++i) {
            ObjectRef obj{ objects[i] };
            if (obj.IsValid() && obj.GetIndex() == index) {
                return obj;
            }
        }
        return {};
    }

    inline int CountInRange(const uintptr_t* objects, int count, float range, const Vec3& from) {
        if (!objects || count <= 0) {
            return 0;
        }

        const ObjectRef local = GetLocalPlayer();
        const Vec3 origin = from.IsZero() && local.IsValid() ? local.GetPosition() : from;
        if (origin.IsZero()) {
            return 0;
        }

        int hits = 0;
        for (int i = 0; i < count; ++i) {
            const ObjectRef obj{ objects[i] };
            if (!obj.IsValid() || obj.IsDead()) {
                continue;
            }
            if (obj.GetPosition().Distance2D(origin) <= range) {
                ++hits;
            }
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
