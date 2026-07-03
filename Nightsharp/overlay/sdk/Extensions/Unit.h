#pragma once

#include "../Core/Game.h"
#include "../Core/Objects.h"
#include "../Enumerations/TurretType.h"
#include "../Events/Dash.h"
#include "../Events/InterruptableSpell.h"
#include "../Events/Teleport.h"
#include "../GameObjects/ObjectManager.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <string>

namespace SDK::Extensions {

    namespace detail {
        static inline constexpr std::array<const char*, 6> kTurretsTierOne = {
            "SRUAP_Turret_Order1", "SRUAP_Turret_Chaos1",
            "TT_OrderTurret5", "TT_ChaosTurret5",
            "HA_AP_OrderTurret", "HA_AP_ChaosTurret"
        };

        static inline constexpr std::array<const char*, 6> kTurretsTierTwo = {
            "SRUAP_Turret_Order2", "SRUAP_Turret_Chaos2",
            "TT_OrderTurret2", "TT_ChaosTurret2",
            "HA_AP_OrderTurret2", "HA_AP_ChaosTurret2"
        };

        static inline constexpr std::array<const char*, 6> kTurretsTierThree = {
            "SRUAP_Turret_Order3", "SRUAP_Turret_Chaos3",
            "TT_OrderTurret1", "TT_ChaosTurret1",
            "HA_AP_OrderTurret3", "HA_AP_ChaosTurret3"
        };

        static inline constexpr std::array<const char*, 4> kTurretsTierFour = {
            "SRUAP_Turret_Order4", "SRUAP_Turret_Chaos4",
            "TT_OrderTurret3", "TT_ChaosTurret3"
        };

        inline float DistanceSquared2D(const Vector3& a, const Vector3& b) {
            return a.To2D().DistanceSqr(b.To2D());
        }

        inline Vector3 ToWorld(const Vector2& position, float y = 0.0f) {
            return Vector3(position.x, y, position.y);
        }

        template <std::size_t N>
        inline bool IsNameIn(const std::string& name, const std::array<const char*, N>& names) {
            return std::find_if(names.begin(), names.end(), [&](const char* value) {
                return value && lstrcmpiA(name.c_str(), value) == 0;
            }) != names.end();
        }

        inline float ClampDot(float value) {
            return std::clamp(value, -1.0f, 1.0f);
        }
    }

    inline int CountAllyHeroesInRange(const AIBaseClient& unit, float range) {
        return unit.CountAllyHeroesInRange(range);
    }

    inline int CountEnemyHeroesInRange(const AIBaseClient& unit, float range) {
        return unit.CountEnemyHeroesInRange(range);
    }

    inline float Distance(const GameObject& source, const GameObject& target) {
        return source.Distance(target);
    }

    inline float Distance(const GameObject& source, const Vector3& position) {
        return source.Distance(position);
    }

    inline float Distance(const GameObject& source, const Vector2& position) {
        return source.Distance(detail::ToWorld(position, source.Position().y));
    }

    inline float Distance(const AIBaseClient& source, const AIBaseClient& target) {
        return source.Distance(target);
    }

    inline float Distance(const AIBaseClient& source, const Vector3& position) {
        return source.Distance(position);
    }

    inline float Distance(const AIBaseClient& source, const Vector2& position) {
        return source.Distance(detail::ToWorld(position, source.Position().y));
    }

    inline float DistanceSquared(const GameObject& source, const GameObject& target) {
        return detail::DistanceSquared2D(source.Position(), target.Position());
    }

    inline float DistanceSquared(const GameObject& source, const Vector3& position) {
        return detail::DistanceSquared2D(source.Position(), position);
    }

    inline float DistanceSquared(const GameObject& source, const Vector2& position) {
        return detail::DistanceSquared2D(source.Position(), detail::ToWorld(position, source.Position().y));
    }

    inline float DistanceSquared(const AIBaseClient& source, const AIBaseClient& target) {
        return detail::DistanceSquared2D(source.Position(), target.Position());
    }

    inline float DistanceSquared(const AIBaseClient& source, const Vector3& position) {
        return detail::DistanceSquared2D(source.Position(), position);
    }

    inline float DistanceSquared(const AIBaseClient& source, const Vector2& position) {
        return detail::DistanceSquared2D(source.Position(), detail::ToWorld(position, source.Position().y));
    }

    inline float DistanceToPlayer(const AIBaseClient& source) {
        return source.DistanceToPlayer();
    }

    inline float DistanceToPlayer(const Vector3& position) {
        const auto player = ObjectManager::Player();
        return player.IsValid() ? player.Distance(position) : FLT_MAX;
    }

    inline float DistanceToPlayer(const Vector2& position) {
        const auto player = ObjectManager::Player();
        return player.IsValid() ? player.Distance(detail::ToWorld(position, player.Position().y)) : FLT_MAX;
    }

    inline int GetRecallTime(const std::string& recallName) {
        if (_stricmp(recallName.c_str(), "recall") == 0) {
            return 8000;
        }
        if (_stricmp(recallName.c_str(), "recallimproved") == 0) {
            return 7000;
        }
        if (_stricmp(recallName.c_str(), "odinrecall") == 0) {
            return 4500;
        }
        if (_stricmp(recallName.c_str(), "odinrecallimproved") == 0 ||
            _stricmp(recallName.c_str(), "superrecall") == 0 ||
            _stricmp(recallName.c_str(), "superrecallimproved") == 0) {
            return 4000;
        }
        return 0;
    }

    inline int GetRecallTime(const AIHeroClient& hero) {
        return GetRecallTime(hero.Spellbook().GetSpell(SpellSlot::Recall).Name());
    }

    inline SpellSlot GetSpellSlot(const AIHeroClient& unit, const char* name) {
        return unit.GetSpellSlot(name);
    }

    inline TurretType GetTurretType(const AITurretClient& turret) {
        const std::string name = turret.CharacterName();
        if (detail::IsNameIn(name, detail::kTurretsTierOne)) {
            return TurretType::TierOne;
        }
        if (detail::IsNameIn(name, detail::kTurretsTierTwo)) {
            return TurretType::TierTwo;
        }
        if (detail::IsNameIn(name, detail::kTurretsTierThree)) {
            return TurretType::TierThree;
        }
        if (detail::IsNameIn(name, detail::kTurretsTierFour)) {
            return TurretType::TierFour;
        }
        return TurretType::Unknown;
    }

    inline bool IsFacing(const AIBaseClient& source, const AIBaseClient& target) {
        if (!source.IsValid() || !target.IsValid()) {
            return false;
        }

        const Vector2 direction = source.Direction().To2D().Normalized();
        const Vector2 toTarget = (target.Position() - source.Position()).To2D().Normalized();
        if (!direction.IsValid() || !toTarget.IsValid() || direction.IsZero() || toTarget.IsZero()) {
            return false;
        }

        const float dot = detail::ClampDot(direction.Dot(toTarget));
        const float angle = std::acos(dot) * (180.0f / 3.14159265358979323846f);
        return angle < 90.0f;
    }

    inline bool IsBothFacing(const AIBaseClient& source, const AIBaseClient& target) {
        return IsFacing(source, target) && IsFacing(target, source);
    }

    inline bool IsMelee(const AIBaseClient& unit) {
        return unit.IsMelee() && unit.AttackRange() < 500.0f;
    }

    inline bool IsRecalling(const AIHeroClient& unit) {
        return unit.IsRecalling();
    }

    inline bool IsUnderAllyTurret(const AIBaseClient& unit) {
        return unit.IsUnderAllyTurret();
    }

    inline bool IsUnderEnemyTurret(const AIBaseClient& unit) {
        return unit.IsUnderEnemyTurret();
    }

    inline bool IsValid(const AIBaseClient& unit) {
        return unit.IsValid();
    }

    inline bool IsValidTarget(const GameObject& unit,
                              float range = FLT_MAX,
                              bool checkTeam = true,
                              const Vector3& from = Vector3()) {
        if (!unit.IsValid() /* || (unit.IsDead() && !unit.IsZombie()) || */
            /* !unit.IsVisible() || !unit.IsTargetable() || unit.IsInvulnerable() */) {
            return false;
        }

        const auto player = ObjectManager::Player();
        if (checkTeam && player.IsValid() && player.Team() == unit.Team()) {
            return false;
        }

        const Vector3 origin = from.IsZero() ? (player.IsValid() ? player.Position() : Vector3()) : from;
        if (range < FLT_MAX && !origin.IsZero()) {
            return detail::DistanceSquared2D(origin, unit.Position()) < (range * range);
        }

        return true;
    }

    inline Events::Dash::DashArgs GetDashInfo(const AIBaseClient& unit) {
        return Events::Dash::GetDashInfo(unit);
    }

    inline bool IsDashing(const AIBaseClient& unit) {
        return Events::Dash::IsDashing(unit);
    }

    inline Events::Teleport::TeleportEventArgs GetTeleportData(const AIBaseClient& unit) {
        return Events::Teleport::GetTeleportData(unit);
    }

    inline bool IsCastingInterruptableSpell(const AIHeroClient& unit, bool checkMovementInterruption = false) {
        return Interrupter::IsCastingInterruptableSpell(unit, checkMovementInterruption);
    }

} // namespace SDK::Extensions
