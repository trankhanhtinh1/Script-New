#pragma once

#include "../../Core/CoreControl.h"
#include "../Core/Game.h"
#include "../Data/GameData.h"
#include "../Extensions/Unit.h"
#include "../GameObjects/ObjectManager.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <limits>
#include <string>

namespace SDK::Core::Utils {

class AutoAttack {
public:
    static bool CanCancelAutoAttack(const AIHeroClient& hero) {
        return hero.CharacterName() != "Kalista";
    }

    static float GetProjectileSpeed(const AIHeroClient& hero) {
        const std::string name = hero.CharacterName();
        if (hero.IsMelee() || name == "Azir" || name == "Velkoz" || name == "Thresh" ||
            (name == "Viktor" && hero.HasBuff("ViktorPowerTransferReturn"))) {
            return std::numeric_limits<float>::max();
        }

        const auto* info = SDK::Data::GameData::GetUnitInfoByName(name);
        return info && info->basicAttackMissileSpeed > 0.0f
            ? info->basicAttackMissileSpeed
            : 2000.0f;
    }

    static float GetRealAutoAttackRange(const AttackableUnit& target) {
        const auto player = SDK::ObjectManager::Player();
        if (target.Compare(player)) {
            return GetRealAutoAttackRange(player, AttackableUnit());
        }
        return GetRealAutoAttackRange(player, target);
    }

    static float GetRealAutoAttackRange(const AIBaseClient& sender, const AttackableUnit& target = AttackableUnit()) {
        if (!sender.IsValid()) {
            return 0.0f;
        }

        float result = sender.AttackRange() + CoreControl::GetBoundingRadius(sender.Address());
        if (target.IsValid()) {
            result += CoreControl::GetBoundingRadius(target.Address());
        }

        if (sender.CharacterName() == "Caitlyn" && target.IsValid()) {
            const AIBaseClient targetBase(target.Handle());
            if (targetBase.HasBuff("caitlynyordletrapinternal")) {
                result += 650.0f;
            }
        }

        return result;
    }

    static float GetTimeToHit(const AttackableUnit& target) {
        const auto player = SDK::ObjectManager::Player();
        const float attackCastDelay = CoreControl::GetAttackWindup(player.Address());
        float time = (attackCastDelay * 1000.0f) - 100.0f + (SDK::Game::Ping() / 2.0f);

        const float missileSpeed = GetProjectileSpeed(player);
        if (std::fabs(missileSpeed - std::numeric_limits<float>::max()) > FLT_EPSILON && target.IsValid()) {
            const AIBaseClient targetBase(target.Handle());
            const Vec3 targetPosition = targetBase.IsValid() ? targetBase.Position() : target.Position();
            time += 1000.0f *
                    std::max(0.0f, player.Distance(targetPosition) - CoreControl::GetBoundingRadius(player.Address())) /
                    std::max(1.0f, missileSpeed);
        }
        return time;
    }

    static bool InAutoAttackRange(const AttackableUnit& target) {
        if (!target.IsValid()) {
            return false;
        }
        return SDK::Extensions::IsValidTarget(target, GetRealAutoAttackRange(target));
    }

    static bool IsAutoAttack(std::string name) {
        ToLower(name);
        return (name.find("attack") != std::string::npos && !Contains(NoAttacks(), name)) ||
               Contains(Attacks(), name);
    }

    static bool IsAutoAttackReset(std::string name) {
        ToLower(name);
        return Contains(AttackResets(), name);
    }

private:
    static void ToLower(std::string& text) {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
    }

    static bool Contains(const char* const* list, const std::string& value) {
        for (int i = 0; list[i]; ++i) {
            if (value == list[i]) {
                return true;
            }
        }
        return false;
    }

    static const char* const* AttackResets() {
        static const char* values[] = {
            "asheq","powerfist","camilleq","camilleq2","vorpalspikes",
            "dariusnoxiantacticsonh","elisespiderw","masochism","ekkoe","fiorae",
            "fizzw","garenq","gangplankqwrapper","gravesmove","hecarimramp",
            "illaoiw","jaxempowertwo","jaycehypercharge","netherblade",
            "kaylee","kindredq","leonashieldofdaybreak","luciane",
            "meditate","monkeykingdoubleattack","mordekaisermaceofspades","nasusq",
            "nautiluspiercinggaze","takedown","reksaiq","renektonpreexecute",
            "rengarq","rengarqemp","riventricleave","settq",
            "shyvanadoubleattack","shyvanadoubleattackdragon",
            "sejuaninorthernwinds","sivirw","talonqattack","talonnoxiandiplomacy",
            "trundletrollsmash","vaynetumble",
            "vie","volibearq","xinzhaoq","xinzhaocombotarget",
            "yorickq","yorickspectral","apheliosinfernumq",
            "itemtitanichydracleave","gravesautoattackrecoilcastedummy",
            nullptr
        };
        return values;
    }

    static const char* const* Attacks() {
        static const char* values[] = {
            "aphelioscalibrumattackmis","aphelioscrescendumattackmis",
            "aphelioscrescendumattackmisin","aphelioscrescendumattackmisout",
            "apheliosgravitumattackmis","apheliosinfernumattackmis",
            "apheliosseverumattackmis",
            "caitlynheadshotmissile","kennenmegaproc","masteryidoublestrike",
            "quinnwenhanced","renektonexecute","renektonsuperexecute",
            "trundleq","viktorqbuff",
            "xinzhaoqthrust1","xinzhaoqthrust2","xinzhaoqthrust3",
            nullptr
        };
        return values;
    }

    static const char* const* NoAttacks() {
        static const char* values[] = {
            "annietibbersbasicattack","annietibbersbasicattack2",
            "asheqattacknoonhit","volleyattackwithsound","volleyattack",
            "azirbasicattacksoldier",
            "dravenattackp_r","dravenattackp_l","dravenattackp_rc","dravenattackp_rq","dravenattackp_lc","dravenattackp_lq",
            "elisespiderlingbasicattack",
            "gravesbasicattackspread","gravesautoattackrecoil",
            "heimertyellowbasicattack","heimertyellowbasicattack2","heimertbluebasicattack","heimerdingerwattack2","heimerdingerwattack2ult",
            "ivernminionbasicattack","ivernminionbasicattack2",
            "kindredwolfbasicattack",
            "malzaharvoidlingbasicattack","malzaharvoidlingbasicattack2","malzaharvoidlingbasicattack3",
            "monkeykingdoubleattack",
            "shyvanadoubleattack","shyvanadoubleattackdragon",
            "talonqattack","talonqdashattack",
            "redcardattack","bluecardattack","goldcardattack",
            "yorickghoulmeleebasicattack","yorickghoulmeleebasicattack2","yorickghoulmeleebasicattack3","yorickbigghoulbasicattack",
            "zoebasicattackspecial1","zoebasicattackspecial2","zoebasicattackspecial3","zoebasicattackspecial4",
            "zyraeplantattack",
            nullptr
        };
        return values;
    }
};

} // namespace SDK::Core::Utils

namespace SDK::Utils {
    using AutoAttack = ::SDK::Core::Utils::AutoAttack;
} // namespace SDK::Utils
