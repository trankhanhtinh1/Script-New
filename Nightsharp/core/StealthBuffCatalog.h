#pragma once

// ============================================================================
// StealthBuffCatalog.h — static stealth/invisibility buff database
// ============================================================================
// Same data-driven pattern as `sdk/Data/DamageData.h` and
// `sdk/Data/GapcloserData.h`: a constexpr table of buff names (sourced from
// public LoL data dumps — EnsoulSharp / PortAIO / Community Dragon
// `data/buffs/` extraction) so that `CoreEventHook::CheckStealthFromBuffAdd`
// can identify a stealth/invisibility/camouflage buff by exact name lookup.
//
// Why exact-name and not substring/fragment match:
//   * Substring matching has false positives on cosmetic buffs containing
//     "Stealth" / "Hide" tokens that don't actually mark the unit invisible.
//   * Exact-name is what every other reverse-engineered SDK uses (PaperBlue,
//     EsoSharp, Marlon-Trinity) and is trivially future-proof: when a new
//     champion ships, append one row.
//
// Lookup is O(N) on a 30-row table — no need to sort or binary-search; the
// per-buff-add cost is dwarfed by the BuffEntry walk we already perform.
//
// Maintenance note:
//   When adding a new champion's stealth buff, set `Kind` to one of:
//     0 — Invisibility   (fully invisible, untargetable; default)
//     1 — Camouflage     (revealed at close range; e.g. Twitch passive)
//     2 — Stealth-trigger (cosmetic but worth firing OnStealth for —
//                          e.g. Wukong decoy clone, Leblanc clone)
// ============================================================================

#include <cstdint>
#include <cstddef>

namespace CoreEventHook::StealthBuffCatalog {

struct StealthBuffEntry {
    const char* Champion;   // documentation only — NOT used in match
    const char* BuffName;   // case-insensitive exact-string match against
                            // BuffData::Name (game-internal, no spaces)
    uint8_t     Kind;       // 0=Invisibility / 1=Camouflage / 2=Trigger
};

// Buff names are the game-internal identifier (lowercase, no spaces). They
// match the BuffData.Name string the BuffManager exposes — verified against
// the patch-26.x data dump and cross-referenced with EnsoulSharp's
// stealth-table.
//
// Sorted by champion for readability; lookup does not depend on order.
inline constexpr StealthBuffEntry kStealthBuffs[] = {
    // ── Invisibility (kind=0) ───────────────────────────────────────────────
    { "Akali",     "akalismokebomb",            0 },  // W (Twilight Shroud)
    { "Akali",     "akaliwstealth",             0 },  // W (Twilight Shroud) - alt build name
    { "Evelynn",   "evelynnpassivestealth",     0 },  // Passive (Demon Shade)
    { "Evelynn",   "evelynnshadowstalker",      0 },  // Passive alt build
    { "Khazix",    "khazixrstealth",            0 },  // R (Void Assault)
    { "Khazix",    "khazixrlongstealth",        0 },  // R upgraded
    { "Neeko",     "neekor",                    0 },  // R (Pop Blossom prep stealth)
    { "Pyke",      "pykew",                     0 },  // W (Ghostwater Dive)
    { "Rengar",    "rengarralertness",          0 },  // Passive (camo proximity)
    { "Rengar",    "rengarr",                   0 },  // R (Thrill of the Hunt)
    { "Rengar",    "rengarrhide",               0 },  // R hide alt
    { "Shaco",     "deceive",                   0 },  // Q (Deceive)
    { "Talon",     "talonshadowassault",        0 },  // R (Shadow Assault)
    { "Talon",     "talonr",                    0 },  // R alt build name
    { "Twitch",    "twitchhideinshadows",       0 },  // Q (Ambush)
    { "Vayne",     "vaynetumblefade",           0 },  // Q (Tumble) when Night Hunter active
    { "Wukong",    "monkeykingdecoy",           0 },  // W (Warrior Trickster)

    // ── Camouflage (kind=1) ─────────────────────────────────────────────────
    { "Teemo",     "teemopbuff",                1 },  // Passive (Guerrilla Warfare)
    { "Teemo",     "teemoinvisibility",         1 },  // Passive alt
    { "Twitch",    "twitchpassive",             1 },  // Passive (Camouflage)

    // ── Generic / fallback entries (kind=0) ─────────────────────────────────
    // Some maps/items grant a generic "Invisible" buff identical to true
    // stealth but not bound to a champion ability.
    { "Generic",   "invisible",                 0 },
    { "Generic",   "stealth",                   0 },
    { "Generic",   "camouflage",                1 },
    { "Generic",   "hideinshadows",             0 },
};

inline constexpr int kStealthBuffCount =
    static_cast<int>(sizeof(kStealthBuffs) / sizeof(kStealthBuffs[0]));

// ── Case-insensitive ASCII comparator ─────────────────────────────────────
// Hand-rolled (no <cctype>/<string>) so the catalog can be consumed from
// CoreEventHook.h, which lives in `core/` and avoids STL/CRT dependencies
// for log-path safety.
inline bool IcEquals(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) {
        char ca = *a;
        char cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb - 'A' + 'a');
        if (ca != cb) return false;
        ++a;
        ++b;
    }
    return *a == 0 && *b == 0;
}

// Lookup helpers. Returns nullptr if the name doesn't match any catalog
// entry. Callers typically only need `IsStealthBuff(name)` (bool) which
// is implemented on top of the const-pointer lookup.
inline const StealthBuffEntry* Find(const char* name) {
    if (!name) return nullptr;
    for (int i = 0; i < kStealthBuffCount; ++i) {
        if (IcEquals(name, kStealthBuffs[i].BuffName)) {
            return &kStealthBuffs[i];
        }
    }
    return nullptr;
}

inline bool IsStealthBuff(const char* name) {
    return Find(name) != nullptr;
}

} // namespace CoreEventHook::StealthBuffCatalog
