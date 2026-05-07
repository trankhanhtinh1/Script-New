#pragma once

// ============================================================================
// SpellbookClient — thin keyed accessor over a unit's 12-slot spellbook
// ============================================================================
// Zero-state wrapper: holds an owner pointer and resolves individual slots
// (Q/W/E/R/D/F/Item1-6/Trinket/Recall) into `SpellDataInstClient` handles on
// demand. Validity is delegated to `CoreSpellBook::Get(owner) != 0` so the
// wrapper correctly reflects hero initialization timing (the spellbook
// pointer becomes non-null only after the client finishes champion load).
// ============================================================================

#include "../../core/CoreAPI.h"
#include "../Enumerations/SpellSlot.h"
#include "SpellDataInstClient.h"

namespace SDK {

class SpellbookClient {
public:
    SpellbookClient() = default;
    explicit SpellbookClient(uintptr_t owner)
        : m_owner(owner) {}

    bool IsValid() const {
        return CoreAPI::SpellBook::Get(m_owner) != 0;
    }

    SpellDataInstClient GetSpell(SpellSlot slot) const {
        return SpellDataInstClient(m_owner, slot);
    }

private:
    uintptr_t m_owner = 0;
};

} // namespace SDK
