#pragma once

#include "../Core/Objects.h"
#include "../Core/Game.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <string>

namespace SDK::Utils::AutoAttack {

namespace detail {
    inline constexpr std::array<const char*, 41> AttackResets = {
        "powerfist","camilleq","camilleq2","vorpalspikes","dariusnoxiantacticsonh","masochism","ekkoe","fiorae",
        "fizzw","garenq","gravesmove","hecarimramp","illaoiw","jaxempowertwo","jaycehypercharge","netherblade",
        "kaylee","kindredq","leonashieldofdaybreak","luciane","meditate","monkeykingdoubleattack","mordekaisermaceofspades","nasusq",
        "nautiluspiercinggaze","takedown","reksaiq","renektonpreexecute","rengarq","riventricleave","shyvanadoubleattack","shyvanadoubleattackdragon",
        "sivirw","talonqattack","trundletrollsmash","vaynetumble","vie","volibearq","xinzhaoq","yorickq",
        "itemtitanichydracleave"
    };

    inline constexpr std::array<const char*, 11> Attacks = {
        "caitlynheadshotmissile","kennenmegaproc","masteryidoublestrike","quinnwenhanced","renektonexecute","renektonsuperexecute",
        "trundleq","viktorqbuff","xinzhaoqthrust1","xinzhaoqthrust2","xinzhaoqthrust3"
    };

    inline constexpr std::array<const char*, 43> NoAttacks = {
        "annietibbersbasicattack","annietibbersbasicattack2","asheqattacknoonhit","volleyattackwithsound","volleyattack","azirbasicattacksoldier",
        "dravenattackp_r","dravenattackp_l","dravenattackp_rc","dravenattackp_rq","dravenattackp_lc","dravenattackp_lq",
        "elisespiderlingbasicattack","gravesbasicattackspread","gravesautoattackrecoil","heimertyellowbasicattack","heimertyellowbasicattack2",
        "heimertbluebasicattack","heimerdingerwattack2","heimerdingerwattack2ult","ivernminionbasicattack","ivernminionbasicattack2",
        "kindredwolfbasicattack","malzaharvoidlingbasicattack","malzaharvoidlingbasicattack2","malzaharvoidlingbasicattack3",
        "monkeykingdoubleattack","shyvanadoubleattack","shyvanadoubleattackdragon","talonqattack","talonqdashattack",
        "redcardattack","bluecardattack","goldcardattack","yorickghoulmeleebasicattack","yorickghoulmeleebasicattack2",
        "yorickghoulmeleebasicattack3","yorickbigghoulbasicattack","zoebasicattackspecial1","zoebasicattackspecial2",
        "zoebasicattackspecial3","zoebasicattackspecial4","zyraeplantattack"
    };

    inline constexpr std::array<const char*, 1> NoCancelChamps = { "Kalista" };

    template<size_t N>
    inline bool ContainsIgnoreCase(const std::array<const char*, N>& list, const std::string& value) {
        return std::any_of(list.begin(), list.end(), [&value](const char* item) {
            return _stricmp(item, value.c_str()) == 0;
        });
    }
}

inline bool CanCancelAutoAttack(const AIHeroClient& hero) {
    return !detail::ContainsIgnoreCase(detail::NoCancelChamps, hero.CharacterName());
}

inline float GetProjectileSpeed(const AIHeroClient& hero) {
    const std::string name = hero.CharacterName();
    if (hero.IsMelee() ||
        _stricmp(name.c_str(), "Azir") == 0 ||
        _stricmp(name.c_str(), "Velkoz") == 0 ||
        _stricmp(name.c_str(), "Thresh") == 0 ||
        (_stricmp(name.c_str(), "Viktor") == 0 && hero.HasBuff("ViktorPowerTransferReturn"))) {
        return FLT_MAX;
    }

    const auto cast = hero.Ref().GetActiveSpellCast();
    const float speed = cast.GetMissileSpeed();
    if (speed > 0.0f && speed < 50000.0f) {
        return speed;
    }

    return 2000.0f;
}

inline float GetRealAutoAttackRange(const GameObject& target) {
    return ObjectManager::Player().GetRealAutoAttackRange(target);
}

inline float GetRealAutoAttackRange(const AIBaseClient& sender, const GameObject& target = GameObject()) {
    return sender.GetRealAutoAttackRange(target);
}

inline float GetTimeToHit(const GameObject& target) {
    const auto player = ObjectManager::Player();
    if (!player.IsValid() || !target.IsValid()) {
        return FLT_MAX;
    }

    float time = (player.AttackCastDelay() * 1000.0f) - 100.0f + (Game::Ping() * 0.5f);
    const float projectileSpeed = GetProjectileSpeed(player);
    if (projectileSpeed != FLT_MAX) {
        time += 1000.0f *
                std::max(0.0f, player.Distance(target.Position()) - player.BoundingRadius()) /
                projectileSpeed;
    }

    return time;
}

inline bool InAutoAttackRange(const GameObject& target) {
    return target.IsValidTarget(GetRealAutoAttackRange(target));
}

inline bool IsAutoAttack(std::string name) {
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ((name.find("attack") != std::string::npos) && !detail::ContainsIgnoreCase(detail::NoAttacks, name)) ||
           detail::ContainsIgnoreCase(detail::Attacks, name);
}

inline bool IsAutoAttackReset(std::string name) {
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return detail::ContainsIgnoreCase(detail::AttackResets, name);
}

} // namespace SDK::Utils::AutoAttack

namespace SDK {
namespace AutoAttack = Utils::AutoAttack;
}
