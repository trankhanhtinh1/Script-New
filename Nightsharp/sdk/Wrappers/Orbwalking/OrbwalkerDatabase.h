#pragma once

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <string>

namespace SDK::OrbwalkingDetail {

inline std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

inline bool Contains(std::initializer_list<const char*> values, const std::string& value) {
    for (const char* item : values) {
        if (item && value == item) {
            return true;
        }
    }
    return false;
}

inline bool Contains(const char* const* values, const std::string& value) {
    if (!values) {
        return false;
    }
    for (int i = 0; values[i]; ++i) {
        if (value == values[i]) {
            return true;
        }
    }
    return false;
}

namespace Database {

inline const char* const* AttackResets() {
    static const char* values[] = {
        "asheq", "camilleq2", "camilleq", "dariusnoxiantacticsonh", "elisespiderw",
        "ekkoe", "fiorae", "fizzw", "gravesmove", "garenq", "gangplankqwrapper",
        "hecarimramp", "illaoiw", "itemtitanichydracleave",
        "jaycehypercharge", "jaxempowertwo", "kaylee", "kindredq", "luciane",
        "leonashieldofdaybreakattack", "leonashieldofdaybreak",
        "mordekaisermaceofspades", "monkeykingdoubleattack", "meditate",
        "masochism", "netherblade", "nautiluspiercinggaze", "nasusq", "powerfist",
        "rengarqemp", "rengarq", "renektonpreexecute", "reksaiq", "riventricleave",
        "settq", "sivirw", "shyvanadoubleattack", "shyvanadoubleattackdragon",
        "sejuaninorthernwinds", "trundletrollsmash", "talonqattack",
        "talonnoxiandiplomacy", "takedown", "vorpalspikes",
        "volibearq", "vie", "vaynetumble", "xinzhaoq", "xinzhaocombotarget",
        "yorickq", "yorickspectral", "apheliosinfernumq", "gravesautoattackrecoilcastedummy",
        nullptr
    };
    return values;
}

inline const char* const* Attacks() {
    static const char* values[] = {
        "aphelioscalibrumattackmis", "aphelioscrescendumattackmis",
        "aphelioscrescendumattackmisin", "aphelioscrescendumattackmisout",
        "apheliosgravitumattackmis", "apheliosinfernumattackmis",
        "apheliosseverumattackmis",
        "caitlynpassivemissile", "itemtitanichydracleave", "itemtiamatcleave",
        "kennenmegaproc", "masteryidoublestrike", "quinnwenhanced",
        "renektonsuperexecute", "renektonexecute", "trundleq", "viktorqbuff",
        "xinzhaoqthrust1", "xinzhaoqthrust2", "xinzhaoqthrust3",
        nullptr
    };
    return values;
}

inline const char* const* NoAttacks() {
    static const char* values[] = {
        "asheqattacknoonhit", "annietibbersbasicattack", "annietibbersbasicattack2",
        "bluecardattack", "dravenattackp_r", "dravenattackp_rc", "dravenattackp_rq",
        "dravenattackp_l", "dravenattackp_lc", "dravenattackp_lq",
        "elisespiderlingbasicattack", "gravesbasicattackspread", "gravesautoattackrecoil",
        "goldcardattack", "heimertyellowbasicattack", "heimertyellowbasicattack2",
        "heimertbluebasicattack", "heimerdingerwattack2", "heimerdingerwattack2ult",
        "ivernminionbasicattack2", "ivernminionbasicattack", "kindredwolfbasicattack",
        "monkeykingdoubleattack", "malzaharvoidlingbasicattack",
        "malzaharvoidlingbasicattack2", "malzaharvoidlingbasicattack3",
        "redcardattack", "shyvanadoubleattackdragon", "shyvanadoubleattack",
        "talonqdashattack", "talonqattack", "volleyattackwithsound", "volleyattack",
        "yorickghoulmeleebasicattack", "yorickghoulmeleebasicattack2",
        "yorickghoulmeleebasicattack3", "yorickbigghoulbasicattack",
        "zyraeplantattack", "zoebasicattackspecial1", "zoebasicattackspecial2",
        "zoebasicattackspecial3", "zoebasicattackspecial4",
        "gravesautoattackrecoilcastedummy", "gravesautoattackrecoil",
        "gravesbasicattackspread",
        nullptr
    };
    return values;
}

} // namespace Database

inline bool IsAutoAttackName(std::string name) {
    name = ToLower(std::move(name));
    if (name.empty() || Contains(Database::NoAttacks(), name)) {
        return false;
    }
    return Contains(Database::Attacks(), name) ||
           name.find("attack") != std::string::npos;
}

inline bool IsAutoAttackResetName(std::string name) {
    name = ToLower(std::move(name));
    return Contains(Database::AttackResets(), name) ||
           name.find("attackreset") != std::string::npos ||
           name.find("takedown") != std::string::npos;
}

} // namespace SDK::OrbwalkingDetail
