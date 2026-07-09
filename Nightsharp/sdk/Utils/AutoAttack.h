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

        if (name == "Kalista") {
            return 2600.0f;
        }

        if (!SDK::Data::GameData::IsLoaded()) {
            return 2000.0f;
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

        float result = sender.AttackRange() + sender.BoundingRadius();
        if (target.IsValid() && !target.IsDead()) {
            const AIBaseClient targetBase(target.Handle());
            if (sender.CharacterName() == "Caitlyn" &&
                (targetBase.HasBuff("CaitlynWSnare") || targetBase.HasBuff("CaitlynEMissile"))) {
                result = 1300.0f;
            } else if (sender.CharacterName() == "Aphelios" &&
                       targetBase.HasBuff("aphelioscalibrumbonusrangedebuff") &&
                       sender.HasBuff("aphelioscalibrumbonusrangebuff")) {
                result = 1800.0f;
            }
            result += target.BoundingRadius();
        }

        return result;
    }

    static float GetTimeToHit(const AttackableUnit& target) {
        const auto player = SDK::ObjectManager::Player();
        const float attackCastDelay = CoreControl::GetAttackWindupMs(player.Address());
        float time = attackCastDelay - 100.0f + (SDK::Game::Ping() / 2.0f);

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
        return Contains(AttackResets(), name) ||
               name.find("attackreset") != std::string::npos ||
               name.find("takedown") != std::string::npos;
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
            "aatroxe","aatroxumbral_dash","aatroxumbraldash",
            "asheq","rangersfocus",
            "belvethq","belvethvoidsurge",
            "powerfist","blitzcranke",
            "briarq","briarw","briarsnackattack",
            "camilleq","camilleq2",
            "vorpalspikes","chogathe",
            "dariusnoxiantacticsonh","dariusw",
            "drmundoe","masochism",
            "ekkoe",
            "elisespiderw",
            "fiorae",
            "fizzw",
            "garenq",
            "gangplankqwrapper",
            "gravesmove","gravesautoattackrecoilcastedummy",
            "gwene","gwenskipnslash",
            "hecarimramp","hecarime",
            "illaoiw",
            "jaxempowertwo",
            "jaycehypercharge",
            "kaisar","kaisakillerinstinct",
            "katarinae","katarinashunpo",
            "ksantepassive","ntofostrikes",
            "netherblade","kassadinw",
            "kaylee",
            "kindredq",
            "leonashieldofdaybreakattack","leonashieldofdaybreak",
            "lucianq","lucianw","luciane","lucianr",
            "malphitew","malphitethunderclap",
            "maokaipassive","maokai_passive","maokaisapmagic",
            "meditate","masteryiw",
            "monkeykingdoubleattack",
            "mordekaisermaceofspades",
            "nasusq",
            "nautilusw","nautiluspiercinggaze",
            "nidaleetakedown","nidaleecougarq","takedown",
            "nilahe",
            "olafw","olaffrenziedstrikes","olaftoughitout",
            "pantheonw","pantheonw1","pantheonempoweredw",
            "quinne","quinnvault",
            "reksaiq",
            "rellw","rellw_dismount","rellferromancymountup",
            "renektonpreexecute",
            "rengarq","rengarqemp",
            "riventricleave",
            "samirap","samiradaredevilimpulse",
            "sejuanie","sejuaninorthernwinds",
            "settq",
            "shyvanadoubleattack","shyvanadoubleattackdragon",
            "sivirw",
            "sonapassive","sona_passive_charged","sonapowerchord",
            "talonqattack","talonnoxiandiplomacy",
            "trundletrollsmash",
            "bluecardlock","goldcardlock","redcardlock","pickacardrecast",
            "vaynetumble",
            "vie","viegow","viegospectralmaw",
            "volibearq",
            "xinzhaoq","xinzhaocombotarget",
            "yorickq","yorickspectral",
            "zacq","zacstretchingstrikes",
            "zerie","zerisparksurge",
            "zoer","zoeportaljump",
            "apheliosinfernumq",
            "hextechrocketbelt","itemhextechrocketbelt","rocketbelt",
            "itemtitanichydracleave",
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
            "caitlynheadshotmissile","caitlynpassivemissile",
            "itemtitanichydracleave","itemtiamatcleave",
            "kennenmegaproc","masteryidoublestrike",
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
            "gravesbasicattackspread","gravesautoattackrecoil","gravesautoattackrecoilcastedummy",
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
