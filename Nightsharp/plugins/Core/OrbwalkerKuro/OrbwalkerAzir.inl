#pragma once

namespace OrbwalkerKuro::OrbwalkingDetail {

using namespace ::SDK;

struct AzirSoldierRuntimeCache {
    int tick = -1;
    int playerNetworkId = 0;
    std::vector<AIMinionClient> soldiers;
};

inline AzirSoldierRuntimeCache& AzirSoldierCache() {
    static AzirSoldierRuntimeCache cache;
    return cache;
}

inline bool IsAzirPlayer(const AIHeroClient& player) {
    return player.IsValid() &&
           AzirSoldierSupport::IsAzirChampionName(player.CharacterName());
}

inline bool IsAzirSandSoldier(const GameObject& obj) {
    return obj.IsValid() &&
           (AzirSoldierSupport::IsSandSoldierName(obj.CharacterName()) ||
            AzirSoldierSupport::IsSandSoldierName(obj.Name()));
}

inline AzirSoldierSupport::Point2 PlanarPoint(const Vector3& position) {
    return { position.x, position.z };
}

inline const std::vector<GameObject>& GetAzirSandSoldiers(
    const AIHeroClient& player
) {
    return AzirSoldierSupport::GetAzirSandSoldiers(player);
}

inline bool IsCommandableAzirSandSoldier(const AIHeroClient& player,
                                         const GameObject& soldier) {
    return IsAzirPlayer(player) && soldier.IsValid() && !soldier.IsDead() &&
           soldier.Team() == player.Team() && IsAzirSandSoldier(soldier) &&
           AzirSoldierSupport::IsCommandable(
               PlanarPoint(player.Position()),
               PlanarPoint(soldier.Position()));
}

inline bool IsStructureTarget(const AttackableUnit& target) {
    using ObjectType = ::Core::Objects::ObjectType;
    switch (target.Type()) {
    case ObjectType::AITurretClient:
    case ObjectType::AITurretCommon:
    case ObjectType::AnimatedBuildingClient:
    case ObjectType::Barracks:
    case ObjectType::BarracksDampenerClient:
    case ObjectType::BuildingClient:
    case ObjectType::HQClient:
    case ObjectType::Turret:
        return true;
    default:
        return false;
    }
}

inline bool IsWardOrTrapTarget(const AttackableUnit& target) {
    if (target.Type() != ::Core::Objects::ObjectType::AIMinionClient) {
        return false;
    }

    const AIMinionClient minion(target.Handle());
    return HasFlag(minion.GetMinionType(), MinionTypes::Ward) ||
           AzirSoldierSupport::IsWardOrTrapName(minion.CharacterName()) ||
           AzirSoldierSupport::IsWardOrTrapName(minion.Name());
}

inline AzirSoldierSupport::TargetKind GetAzirSoldierTargetKind(
    const AttackableUnit& target
) {
    if (IsStructureTarget(target)) {
        return AzirSoldierSupport::TargetKind::Structure;
    }
    if (IsWardOrTrapTarget(target)) {
        return AzirSoldierSupport::TargetKind::WardOrTrap;
    }
    return AzirSoldierSupport::TargetKind::OrdinaryUnit;
}

inline int AzirSoldierAttackCount(const AIHeroClient& player,
                                  const AttackableUnit& target,
                                  float rangeScale = 1.0f) {
    if (!IsAzirPlayer(player) || !target.IsValid() ||
        (target.IsDead() && !target.IsZombie()) ||
        !AzirSoldierSupport::CanUseSoldierAttack(
            GetAzirSoldierTargetKind(target))) {
        return 0;
    }

    const auto playerPoint = PlanarPoint(player.Position());
    const auto targetPoint = PlanarPoint(target.Position());
    const float targetRadius = target.BoundingRadius();
    const float attackRange = AzirSoldierSupport::kPrimaryAttackRange *
                              std::max(0.0f, rangeScale);
    int count = 0;
    for (const auto& soldier : GetAzirSandSoldiers(player)) {
        if (!AzirSoldierSupport::IsCommandable(
                playerPoint, PlanarPoint(soldier.Position())) ||
            !AzirSoldierSupport::CanReachPrimaryTarget(
                PlanarPoint(soldier.Position()),
                targetPoint,
                targetRadius * std::max(0.0f, rangeScale),
                attackRange)) {
            continue;
        }
        ++count;
    }
    return count;
}

inline bool IsTargetWithinOrdinaryAttackRange(const AIHeroClient& player,
                                              const AttackableUnit& target,
                                              float rangeScale = 1.0f) {
    if (!player.IsValid() || !target.IsValid()) {
        return false;
    }
    const float range = GetRealAutoAttackRange(player, target) *
                        std::max(0.0f, rangeScale);
    return player.Position().DistanceSqr2D(target.Position()) <= range * range;
}

inline bool IsTargetWithinCurrentAttackRange(const AIHeroClient& player,
                                             const AttackableUnit& target,
                                             float rangeScale = 1.0f) {
    return IsTargetWithinOrdinaryAttackRange(player, target, rangeScale) ||
           AzirSoldierAttackCount(player, target, rangeScale) > 0;
}

inline bool WillUseAzirSoldierAttack(const AIHeroClient& player,
                                     const AttackableUnit& target) {
    // If both Azir and a soldier can reach the target, the live game replaces
    // Azir's ordinary attack with the soldier command.
    return AzirSoldierAttackCount(player, target) > 0;
}

inline float GetCurrentAutoAttackDamage(const AIHeroClient& player,
                                        const AIBaseClient& target) {
    if (!player.IsValid() || !target.IsValid()) {
        return 0.0f;
    }

    const AttackableUnit attackTarget(target.Handle());
    const int soldierCount = AzirSoldierAttackCount(player, attackTarget);
    const int wRank = player.GetSpell(SpellSlot::W).Level();
    if (soldierCount <= 0 || wRank <= 0) {
        return Damage::GetAutoAttackDamage(player, target);
    }

    const float rawDamage = AzirSoldierSupport::MultiSoldierRawDamage(
        player.Level(), wRank, player.AP(), soldierCount);
    float damage = player.CalculateMagicDamage(target, rawDamage);

    // W applies direct on-hit effects once to the primary target at 50%.
    // The delta avoids adding Azir's ordinary physical attack, which is
    // replaced entirely whenever a soldier can perform the stab.
    const float ordinaryWithPassives =
        Damage::GetAutoAttackDamage(player, target, true);
    const float ordinaryWithoutPassives =
        Damage::GetAutoAttackDamage(player, target, false);
    const float directOnHitDamage =
        std::max(0.0f, ordinaryWithPassives - ordinaryWithoutPassives);
    damage += directOnHitDamage * AzirSoldierSupport::kOnHitEffectiveness;
    return damage;
}

inline bool IsAzirSoldierAttackEvent(
    const Events::ProcessSpellEventArgs& args
) {
    return AzirSoldierSupport::IsSoldierAttackSpellName(args.SpellName) ||
           AzirSoldierSupport::IsSoldierAttackSpellName(args.MissileName) ||
           AzirSoldierSupport::IsSoldierAttackSpellName(args.ScriptName) ||
           AzirSoldierSupport::IsSoldierAttackSpellName(args.SpellSlotName) ||
           AzirSoldierSupport::IsSoldierAttackSpellName(args.PayloadSpellName) ||
           AzirSoldierSupport::IsSoldierAttackSpellName(args.PayloadMissileName);
}

inline bool IsOwnedAzirSoldierSender(
    const AIHeroClient& player,
    const ::Core::Events::ObjectInfo& sender
) {
    if (!IsAzirPlayer(player) || !sender.IsValid() ||
        sender.Team != static_cast<std::uint32_t>(player.Team()) ||
        (!AzirSoldierSupport::IsSandSoldierName(sender.CharacterName) &&
         !AzirSoldierSupport::IsSandSoldierName(sender.Name))) {
        return false;
    }

    for (const auto& soldier : GetAzirSandSoldiers(player)) {
        if (sender.NetworkId != 0 &&
            sender.NetworkId != 0xFFFFFFFFu &&
            sender.NetworkId == static_cast<std::uint32_t>(soldier.NetworkId())) {
            return true;
        }
    }

    // Object lifecycle and cast bridges may expose the Sand Soldier before
    // ObjectManager has refreshed its handle.  The team/name/tether fallback
    // still rejects enemy soldiers and distant allied Azirs' pets.
    return AzirSoldierSupport::IsCommandable(
        PlanarPoint(player.Position()),
        { sender.Position.x, sender.Position.z });
}

inline bool IsAzirSoldierAttackMissileName(const Events::ObjectEventArgs& args) {
    return AzirSoldierSupport::IsSoldierAttackSpellName(args.SpellName) ||
           AzirSoldierSupport::IsSoldierAttackSpellName(args.MissileName);
}

} // namespace OrbwalkerKuro::OrbwalkingDetail
