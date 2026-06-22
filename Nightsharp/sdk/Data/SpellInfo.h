#pragma once

// ============================================================================
// SpellInfo.h - Static spell/missile description loaded from SpellData.json
// ----------------------------------------------------------------------------
// Port of LView/SpellInfo.{h,cpp}. Same flag layout, same field set.
//
// NOTE: this struct is the *file-loaded* metadata for skillshots & missiles —
// it is intentionally distinct from `SDK::Data::SpellData` (which is the
// hard-coded skillshot DB used by the EnsoulSharp prediction wrappers).
// They cohabit cleanly because the names differ and both live in
// `namespace SDK::Data`.
// ============================================================================

#include <string>

namespace SDK::Data {

// Bitfield describing which kinds of units the spell is allowed to interact
// with. Values come straight from the LView JSON files (LeagueSandbox /
// Olaf-spell extractor) so the integer constants must NOT be reassigned.
enum SpellFlags : int {
    // Flags from the game data files
    AffectAllyChampion        = 1,
    AffectEnemyChampion       = 1 << 1,
    AffectAllyLaneMinion      = 1 << 2,
    AffectEnemyLaneMinion     = 1 << 3,
    AffectAllyWard            = 1 << 4,
    AffectEnemyWard           = 1 << 5,
    AffectAllyTurret          = 1 << 6,
    AffectEnemyTurret         = 1 << 7,
    AffectAllyInhibs          = 1 << 8,
    AffectEnemyInhibs         = 1 << 9,
    AffectAllyNonLaneMinion   = 1 << 10,
    AffectJungleMonster       = 1 << 11,
    AffectEnemyNonLaneMinion  = 1 << 12,
    AffectAlwaysSelf          = 1 << 13,
    AffectNeverSelf           = 1 << 14,

    // Custom flags set by us. These cannot be unpacked from the game files.
    ProjectedDestination      = 1 << 22,

    AffectAllyMob             = AffectAllyLaneMinion  | AffectAllyNonLaneMinion,
    AffectEnemyMob            = AffectEnemyLaneMinion | AffectEnemyNonLaneMinion | AffectJungleMonster,
    AffectAllyGeneric         = AffectAllyMob         | AffectAllyChampion,
    AffectEnemyGeneric        = AffectEnemyMob        | AffectEnemyChampion,
};

struct SpellInfo {
    std::string name;
    std::string icon;

    SpellFlags flags      = static_cast<SpellFlags>(0);
    float      delay      = 0.0f;
    float      castRange  = 0.0f;
    float      castRadius = 0.0f;
    float      width      = 0.0f;
    float      height     = 0.0f;
    float      speed      = 0.0f;
    float      travelTime = 0.0f;

    SpellInfo* AddFlags(SpellFlags additional) {
        flags = static_cast<SpellFlags>(static_cast<int>(flags) | static_cast<int>(additional));
        return this;
    }

    bool HasFlag(SpellFlags f) const {
        return (static_cast<int>(flags) & static_cast<int>(f)) != 0;
    }
};

} // namespace SDK::Data
