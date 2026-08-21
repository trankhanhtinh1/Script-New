#pragma once

// Champion-specific decision contract for KuroAIO AI plugins.
//
// AIChampionEngine owns only reusable plumbing (prediction objects, menus,
// autonomous target selection, safety scoring and event subscription).  A
// controller with OwnsDecisionLoop=true is responsible for every
// combat/farm/flee decision for its champion.  This keeps a generated
// Q-W-E-R profile from being mistaken for a completed one-trick plugin.

#include "AIChampionProfile.h"

#include <cstddef>

namespace Plugins::KuroAIO::AI {

struct ChampionController {
    SDK::ChampionId ChampionId = SDK::ChampionId::Unknown;
    const char* ControllerId = "";
    const char* KitRevision = "";
    const char* ResearchArtifact = "";
    const char* ImplementationSummary = "";

    const char* const* Scenarios = nullptr;
    std::size_t ScenarioCount = 0;

    // Full one-trick controllers set this to true.  When true, the shared
    // engine never falls back to generic combo/farm logic after OnUpdate.
    bool OwnsDecisionLoop = false;

    void (*OnLoad)() = nullptr;
    void (*OnUnload)() = nullptr;
    void (*BuildMenu)(Menu* root) = nullptr;
    bool (*OnUpdate)(Mode mode, const AIHeroClient& target) = nullptr;
    void (*OnDraw)() = nullptr;

    void (*OnProcessSpell)(const SDK::Events::ProcessSpellEventArgs& args) = nullptr;
    void (*OnDoCast)(const SDK::Events::ProcessSpellEventArgs& args) = nullptr;
    void (*OnBuffAdd)(const SDK::Events::BuffEventArgs& args) = nullptr;
    void (*OnBuffRemove)(const SDK::Events::BuffEventArgs& args) = nullptr;
    void (*OnBeforeAttack)(SDK::OrbwalkingActionArgs& args) = nullptr;
    void (*OnAfterAttack)(SDK::OrbwalkingActionArgs& args) = nullptr;
    void (*OnGapcloser)(
        const SDK::Events::Gapcloser::GapCloserEventArgs& args) = nullptr;
    void (*OnInterruptable)(
        const SDK::Events::InterruptableSpell::InterruptableTargetEventArgs& args) = nullptr;
    void (*OnObjectCreate)(const SDK::Events::ObjectEventArgs& args) = nullptr;
    void (*OnObjectDelete)(const SDK::Events::ObjectEventArgs& args) = nullptr;
    // Dedicated missile lifecycle hooks are intentionally separate from
    // generic object creation.  Return-projectile champions (Ahri, Sivir,
    // Draven, etc.) need the spell payload captured by the missile hook and
    // must not rely on a best-effort object-name scan.
    void (*OnMissileCreate)(const SDK::Events::ObjectEventArgs& args) = nullptr;
    void (*OnMissileDelete)(const SDK::Events::ObjectEventArgs& args) = nullptr;
};

} // namespace Plugins::KuroAIO::AI
