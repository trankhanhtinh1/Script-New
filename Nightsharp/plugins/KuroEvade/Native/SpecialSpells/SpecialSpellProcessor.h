#pragma once

#include "AllChampions.h"
#include "Braum.h"
#include "Draven.h"
#include "Ekko.h"
#include "Heimerdinger.h"
#include "Irelia.h"
#include "Jinx.h"
#include "Lucian.h"
#include "Malphite.h"
#include "Maokai.h"
#include "Mordekaiser.h"
#include "Nami.h"
#include "Pyke.h"
#include "Seraphine.h"
#include "Sion.h"
#include "Sylas.h"
#include "Syndra.h"
#include "Taric.h"
#include "Velkoz.h"
#include "Warwick.h"
#include "Yuumi.h"
#include "Zac.h"
#include "Zed.h"
#include "Ziggs.h"
#include "Zilean.h"

namespace Plugins::KuroEvade::SpecialSpells {

inline ProcessResult ProcessCast(const SDK::AIBaseClient& caster,
                                 const SDK::Events::ProcessSpellEventArgs& args,
                                 const Database::SpellData& source,
                                 SpellLookupFn lookup) {
    ProcessResult result;
    result.Data = source;

    const CastContext context = MakeCastContext(caster, args, source, lookup);
    if (AllChampions::ProcessCast(context, result) ||
        Braum::ProcessCast(context, result) ||
        Draven::ProcessCast(context, result) ||
        Ekko::ProcessCast(context, result) ||
        Heimerdinger::ProcessCast(context, result) ||
        Irelia::ProcessCast(context, result) ||
        Jinx::ProcessCast(context, result) ||
        Lucian::ProcessCast(context, result) ||
        Malphite::ProcessCast(context, result) ||
        Maokai::ProcessCast(context, result) ||
        Mordekaiser::ProcessCast(context, result) ||
        Nami::ProcessCast(context, result) ||
        Pyke::ProcessCast(context, result) ||
        Seraphine::ProcessCast(context, result) ||
        Sion::ProcessCast(context, result) ||
        Sylas::ProcessCast(context, result) ||
        Syndra::ProcessCast(context, result) ||
        Taric::ProcessCast(context, result) ||
        Velkoz::ProcessCast(context, result) ||
        Warwick::ProcessCast(context, result) ||
        Yuumi::ProcessCast(context, result) ||
        Zac::ProcessCast(context, result) ||
        Zed::ProcessCast(context, result) ||
        Ziggs::ProcessCast(context, result) ||
        Zilean::ProcessCast(context, result)) {
        return result;
    }

    return result;
}

inline void ProcessMissile(const SDK::AIBaseClient& caster,
                           const SDK::MissileClient& missile,
                           Database::SpellData& data) {
    if (AllChampions::ProcessMissile(caster, missile, data) ||
        Jinx::ProcessMissile(caster, missile, data) ||
        Nami::ProcessMissile(caster, missile, data)) {
        return;
    }
}

inline void BeginUpdate() {
    Yuumi::BeginUpdate();
}

inline bool UpdateSkillshot(SDK::Skillshot& skillshot) {
    if (!Sion::Update(skillshot)) {
        return false;
    }
    Seraphine::Update(skillshot);
    Yuumi::Update(skillshot);
    return true;
}

inline void EndUpdate() {
    Yuumi::EndUpdate();
}

inline void ClearState() {
    Sion::Clear();
    Yuumi::Clear();
}

} // namespace Plugins::KuroEvade::SpecialSpells
