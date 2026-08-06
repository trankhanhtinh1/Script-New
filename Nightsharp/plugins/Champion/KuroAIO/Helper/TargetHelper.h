#pragma once

#include "../../../../SDK/SDK.h"
#include "../../../Core/KuroTargetSelector/KuroTargetSelectorContracts.h"

#include <algorithm>
#include <cfloat>
#include <cstring>
#include <vector>

namespace Plugins::KuroAIO {

inline AIHeroClient Player() {
    return GameObjects::Player();
}

inline std::string GetObjectName(const GameObject& object) {
    if (!object.IsValid()) return {};
    return object.Name();
}

inline std::string GetObjectCharacterName(const GameObject& object) {
    if (!object.IsValid()) return {};
    return object.CharacterName();
}

inline bool Recent(int tick, int windowMs) {
    const int now = SDK::Variables::TickCount();
    return tick > 0 && now >= tick && now - tick <= windowMs;
}

inline bool EqualsIgnoreCase(const char* left, const char* right) {
    return left && right && left[0] && right[0] && _stricmp(left, right) == 0;
}

inline bool ValidUnit(const AttackableUnit& unit) {
    return unit.IsValid() && !unit.IsDead() && unit.Health() > 0.0f && unit.IsTargetable();
}

inline bool ValidTarget(const AIBaseClient& unit, float range = FLT_MAX) {
    return ValidUnit(unit) && Extensions::IsValidTarget(unit, range, true);
}

inline bool ValidHeroTarget(const AIHeroClient& hero, float range = FLT_MAX) {
    return ValidUnit(hero) && Extensions::IsValidTarget(hero, range, true);
}

inline std::vector<AIHeroClient> EnemyHeroes(float range) {
    std::vector<AIHeroClient> result;
    for (const auto& enemy : GameObjects::EnemyHeroesFrame()) {
        if (ValidHeroTarget(enemy, range)) {
            result.push_back(enemy);
        }
    }
    return result;
}

inline std::vector<AIHeroClient> EnemyHeroesByHealth(float range) {
    auto result = EnemyHeroes(range);
    std::sort(result.begin(), result.end(), [](const AIHeroClient& a, const AIHeroClient& b) {
        return a.Health() < b.Health();
    });
    return result;
}

// This is the small adapter shared by KuroAIO target callers.  The selector
// owns the decision; KuroAIO only supplies the action context and identity
// hints it already tracks.  Keeping request construction here leaves the
// existing SDK path below intact when the Kuro service is not loaded.
inline SDK::KuroTargetSelector::TargetRequest MakeKuroTargetRequest(
    float range,
    SDK::DamageType damageType,
    SDK::KuroTargetSelector::TargetPurpose purpose,
    SDK::KuroTargetSelector::DecisionPhase phase =
        SDK::KuroTargetSelector::DecisionPhase::Planning,
    int preferredTargetId = 0,
    int lockedTargetId = 0) {
    using namespace SDK::KuroTargetSelector;

    TargetRequest request{};
    request.Purpose = purpose;
    request.Phase = phase;
    request.Range = range;
    request.Source = Player().IsValid() ? Player().Position() : SDK::Vector3();
    request.Damage.Type = damageType;
    request.Damage.IncludeShields = true;
    // Shield channels affect killability; IgnoreShields remains reserved for
    // the invulnerability gate, not the survivability score.
    request.Damage.IgnoreShields = true;
    request.Route.Kind = RouteKind::NonProjectile;
    request.Route.Start = request.Source;
    request.Route.RequireVisible = true;
    request.Route.TargetableAtExecution = true;
    request.PreferredTargetId = preferredTargetId;
    request.LockedTargetId = lockedTargetId;
    request.AllowFallback = true;
    request.RespectManualSelection = true;
    request.RequireVisible = true;
    return request;
}

inline AIHeroClient GetTarget(float range, DamageType damageType) {
    if (auto* kuro = SDK::KuroTargetSelector::ActiveService()) {
        const auto state = kuro->GetSelectionState();
        if (!state.Suspended) {
            auto request = MakeKuroTargetRequest(
                range,
                damageType,
                SDK::KuroTargetSelector::TargetPurpose::General,
                SDK::KuroTargetSelector::DecisionPhase::Planning,
                state.PreferSelectedTarget ? state.SelectedNetworkId : 0,
                state.IncumbentNetworkId);
            request.RespectManualSelection = state.PreferSelectedTarget;

            const auto decision = kuro->Select(request);
            if (decision.Legal && ValidHeroTarget(decision.Target, range)) {
                return decision.Target;
            }
        }
    }

    // Preserve the legacy SDK selector and health fallback whenever Kuro is
    // unavailable, explicitly suspended, or has no legal candidate. Resolve
    // the SDK implementation by name so this fallback cannot recurse through
    // the active Kuro implementation.
    if (auto* selector = SDK::TargetSelector::GetTargetSelector("SDK")) {
        const auto selected = selector->GetTarget(range, damageType);
        if (ValidHeroTarget(selected, range)) {
            return selected;
        }
    }

    const auto enemies = EnemyHeroesByHealth(range);
    return enemies.empty() ? AIHeroClient() : enemies.front();
}

inline AIHeroClient GetPhysicalTarget(float range) {
    return GetTarget(range, DamageType::Physical);
}

inline AIHeroClient GetMagicalTarget(float range) {
    return GetTarget(range, DamageType::Magical);
}

} // namespace Plugins::KuroAIO
