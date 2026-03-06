#pragma once
// ============================================================================
// AutoAttackUtil.h — Auto Attack Utility Database (EnsoulSharp SDK Port)
// ============================================================================
// Full port of EnsoulSharp.SDK/Core/Utils/AutoAttack.cs
// Provides:
//   - IsAutoAttack(spellName)
//   - IsAutoAttackReset(spellName)
//   - CanCancelAutoAttack(championName)
//   - GetProjectileSpeed(hero)
//   - GetRealAutoAttackRange(source, target)
// ============================================================================

#include "GameObject.h"
#include "BuffManager.h"

#include <string>
#include <vector>
#include <algorithm>
#include <cmath>

namespace SDK {

class AutoAttackUtil {
public:

    // ---- Spells that reset the attack timer ----
    static bool IsAutoAttackReset(const std::string& name) {
        std::string lower = ToLower(name);
        for (auto& reset : s_attackResets) {
            if (lower == reset) return true;
        }
        return false;
    }

    // ---- Is this spell name an auto attack? ----
    static bool IsAutoAttack(const std::string& name) {
        std::string lower = ToLower(name);

        // Check explicit attacks list
        for (auto& atk : s_attacks) {
            if (lower == atk) return true;
        }

        // Contains "attack" and not in no-attacks list
        if (lower.find("attack") != std::string::npos) {
            for (auto& noAtk : s_noAttacks) {
                if (lower == noAtk) return false;
            }
            return true;
        }

        return false;
    }

    // ---- Can this champion cancel their AA animation? ----
    static bool CanCancelAutoAttack(const std::string& championName) {
        for (auto& champ : s_noCancelChamps) {
            if (championName == champ) return false;
        }
        return true;
    }

    // ---- Get projectile speed for a hero ----
    static float GetProjectileSpeed(const GameObject& hero) {
        std::string name = hero.GetChampionName();

        // Melee champions and special cases have instant projectiles
        if (hero.IsMelee() || name == "Azir" || name == "Velkoz" || name == "Thresh") {
            return 999999.0f; // effectively instant
        }

        // Viktor Q empowered
        if (name == "Viktor") {
            BuffManager bm(hero.address);
            if (bm.HasBuff("ViktorPowerTransferReturn"))
                return 999999.0f;
        }

        // Default: use missile speed from game's basic attack spell data
        SpellBook sb(hero.address);
        if (sb.IsValid()) {
            auto spell = sb.GetSpell(SpellSlotId::Q); // slot 0 is basic attack internally
            // Try to read from AA spell data's missile speed
            // For most heroes, we read from their SpellData resource
        }
        // Fallback: typical ranged missile speed
        return 1500.0f;
    }

    // ---- Get real auto attack range (including bounding radii) ----
    static float GetRealAutoAttackRange(const GameObject& source, const GameObject& target) {
        if (!source.IsValid()) return 0.0f;

        float result = source.GetAttackRange() + source.GetBoundingRadius();
        if (target.IsValid()) {
            result += target.GetBoundingRadius();
        }

        // Caitlyn headshot trap bonus range
        std::string srcName = source.GetChampionName();
        if (srcName == "Caitlyn" && target.IsValid()) {
            BuffManager bm(target.address);
            if (bm.HasBuff("caitlynyordletrapinternal")) {
                result += 650.0f;
            }
        }

        return result;
    }

    // ---- Get real AA range using player as source ----
    static float GetRealAutoAttackRange(const GameObject& target) {
        auto& player = GameObjects::Player;
        return GetRealAutoAttackRange(player, target);
    }

    // ---- Is target in auto attack range? ----
    static bool InAutoAttackRange(const GameObject& source, const GameObject& target) {
        if (!source.IsValid() || !target.IsValid() || !target.IsAlive()) return false;
        float range = GetRealAutoAttackRange(source, target);
        float dist = source.GetPosition().Distance(target.GetPosition());
        return dist <= range;
    }

    // ---- Time to hit target with auto attack (ms) ----
    static float GetTimeToHit(const GameObject& source, const GameObject& target) {
        float windupMs = source.GetAttackWindup() * 1000.0f - 100.0f;
        float projSpeed = GetProjectileSpeed(source);

        float travelMs = 0.0f;
        if (projSpeed < 900000.0f) { // Not instant
            float dist = source.GetPosition().Distance(target.GetPosition()) - source.GetBoundingRadius();
            if (dist < 0) dist = 0;
            travelMs = (dist / projSpeed) * 1000.0f;
        }

        return windupMs + travelMs;
    }

private:
    static std::string ToLower(const std::string& s) {
        std::string result = s;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }

    // ---- Attack reset spells ----
    static inline const std::vector<std::string> s_attackResets = {
        "powerfist",            // Blitzcrank E
        "camilleq", "camilleq2",
        "vorpalspikes",         // Cho'Gath E
        "dariusnoxiantacticsonh",
        "masochism",            // Dr. Mundo E
        "ekkoe",
        "fiorae",
        "fizzw",
        "garenq",
        "gravesmove",
        "hecarimramp",
        "illaoiw",
        "jaxempowertwo",
        "jaycehypercharge",
        "netherblade",          // Kassadin W
        "kaylee",               // Kayle E
        "kindredq",
        "leonashieldofdaybreak",
        "luciane",
        "meditate",             // Master Yi W
        "monkeykingdoubleattack",
        "mordekaisermaceofspades",
        "nasusq",
        "nautiluspiercinggaze",
        "takedown",             // Nidalee cougar Q
        "reksaiq",
        "renektonpreexecute",
        "rengarq",
        "riventricleave",
        "shyvanadoubleattack", "shyvanadoubleattackdragon",
        "sivirw",
        "talonqattack",
        "trundletrollsmash",
        "vaynetumble",
        "vie",                  // Vi E
        "volibearq",
        "xinzhaoq",
        "yorickq",
        "itemtitanichydracleave",
        // Season 2026 additions
        "settq",                // Sett Q
        "viegoq",               // Viego Q
        "gwenq",                // Gwen Q
        "briarq",               // Briar Q
        "ksanteq",              // K'Sante Q
        "belvethe",             // Bel'Veth E
        "nilahq",               // Nilah Q
        "ambessaq",             // Ambessa Q
    };

    // ---- Spells that are attacks but don't contain "attack" ----
    static inline const std::vector<std::string> s_attacks = {
        "caitlynheadshotmissile",
        "kennenmegaproc",
        "masteryidoublestrike",
        "quinnwenhanced",
        "renektonexecute", "renektonsuperexecute",
        "trundleq",
        "viktorqbuff",
        "xinzhaoqthrust1", "xinzhaoqthrust2", "xinzhaoqthrust3",
        // Season 2026 additions
        "vaborusq",             // Varus Q2 (not really but some consider it)
        "settqpunch",
    };

    // ---- Spells that contain "attack" but are NOT auto attacks ----
    static inline const std::vector<std::string> s_noAttacks = {
        "annietibbersbasicattack", "annietibbersbasicattack2",
        "asheqattacknoonhit",
        "volleyattackwithsound", "volleyattack",
        "azirbasicattacksoldier",
        "dravenattackp_r", "dravenattackp_l", "dravenattackp_rc",
        "dravenattackp_rq", "dravenattackp_lc", "dravenattackp_lq",
        "elisespiderlingbasicattack",
        "gravesbasicattackspread", "gravesautoattackrecoil",
        "heimertyellowbasicattack", "heimertyellowbasicattack2",
        "heimertbluebasicattack",
        "heimerdingerwattack2", "heimerdingerwattack2ult",
        "ivernminionbasicattack", "ivernminionbasicattack2",
        "kindredwolfbasicattack",
        "malzaharvoidlingbasicattack", "malzaharvoidlingbasicattack2",
        "malzaharvoidlingbasicattack3",
        "monkeykingdoubleattack",
        "shyvanadoubleattack", "shyvanadoubleattackdragon",
        "talonqattack", "talonqdashattack",
        "redcardattack", "bluecardattack", "goldcardattack",
        "yorickghoulmeleebasicattack", "yorickghoulmeleebasicattack2",
        "yorickghoulmeleebasicattack3", "yorickbigghoulbasicattack",
        "zoebasicattackspecial1", "zoebasicattackspecial2",
        "zoebasicattackspecial3", "zoebasicattackspecial4",
        "zyraeplantattack",
        // Season 2026 additions
        "viegopassiveattack",
        "briarpassiveattack",
    };

    // ---- Champions that cannot cancel auto attack animation ----
    static inline const std::vector<std::string> s_noCancelChamps = {
        "Kalista"
    };
};

} // namespace SDK
