#pragma once

#include <cstddef>
#include <cstring>

// ============================================================================
// SDK::Items — Named Riot item-id constants + script-name lookup.
// ============================================================================
//
// Usage:
//
//   if (hero.HasItem(SDK::Items::Tiamat))        { ... }
//   if (hero.HasItem(SDK::Items::InfinityEdge))  { ... }
//
//   // Or by script name (case-insensitive, matches the CommunityDragon/DDragon
//   // id that the game loads from bin files):
//   if (hero.HasItem("Tiamat"))                  { ... }
//   if (hero.HasItem("InfinityEdge"))            { ... }
//
// Constants are drawn from https://raw.communitydragon.org/latest/ so they
// track the current League patch. If the game adds or renames an item, append
// a new line here; existing entries rarely change id.
//
// The integer value is the Riot item id (same one seen in the in-client URL,
// DDragon, and CDragon). AIBaseClient::HasItem(int) converts that id into a
// runtime inventory probe (catalog + buff fallback), so adding a constant
// here is enough to make `hero.HasItem(SDK::Items::X)` compile.
// ============================================================================

namespace SDK {
namespace Items {

    // ── Consumables & Wards ──
    inline constexpr int HealthPotion                   = 2003;
    inline constexpr int TotalBiscuitOfEverlastingWill  = 2010;
    inline constexpr int KircheisShard                  = 2015;
    inline constexpr int RefillablePotion               = 2031;
    inline constexpr int CorruptingPotion               = 2033;
    inline constexpr int ControlWard                    = 2055;
    inline constexpr int ElixirOfIron                   = 2138;
    inline constexpr int ElixirOfSorcery                = 2139;
    inline constexpr int ElixirOfWrath                  = 2140;

    // ── Starter / Component ──
    inline constexpr int BootsOfSpeed                   = 1001;
    inline constexpr int FaerieCharm                    = 1004;
    inline constexpr int RejuvenationBead               = 1006;
    inline constexpr int GiantsBelt                     = 1011;
    inline constexpr int CloakOfAgility                 = 1018;
    inline constexpr int BlastingWand                   = 1026;
    inline constexpr int SapphireCrystal                = 1027;
    inline constexpr int RubyCrystal                    = 1028;
    inline constexpr int ClothArmor                     = 1029;
    inline constexpr int ChainVest                      = 1031;
    inline constexpr int NullMagicMantle                = 1033;
    inline constexpr int LongSword                      = 1036;
    inline constexpr int Pickaxe                        = 1037;
    inline constexpr int BFSword                        = 1038;
    inline constexpr int Dagger                         = 1042;
    inline constexpr int RecurveBow                     = 1043;
    inline constexpr int AmplifyingTome                 = 1052;
    inline constexpr int VampiricScepter                = 1053;
    inline constexpr int DoransShield                   = 1054;
    inline constexpr int DoransBlade                    = 1055;
    inline constexpr int DoransRing                     = 1056;
    inline constexpr int NegatronCloak                  = 1057;
    inline constexpr int NeedlesslyLargeRod             = 1058;
    inline constexpr int DarkSeal                       = 1082;
    inline constexpr int Cull                           = 1083;

    // ── Boots ──
    inline constexpr int BerserkersGreaves              = 3006;
    inline constexpr int BootsOfSwiftness               = 3009;
    inline constexpr int SorcerersShoes                 = 3020;
    inline constexpr int PlatedSteelcaps                = 3047;
    inline constexpr int MercurysTreads                 = 3111;
    inline constexpr int IonianBootsOfLucidity          = 3158;

    // ── AD / Crit / On-hit legendaries ──
    inline constexpr int InfinityEdge                   = 3031;
    inline constexpr int MortalReminder                 = 3033;
    inline constexpr int LastWhisper                    = 3035;
    inline constexpr int LordDominiksRegards            = 3036;
    inline constexpr int PhantomDancer                  = 3046;
    inline constexpr int SterakGage                     = 3053;
    inline constexpr int BlackCleaver                   = 3071;
    inline constexpr int Bloodthirster                  = 3072;
    inline constexpr int RavenousHydra                  = 3074;
    inline constexpr int Tiamat                         = 3077;
    inline constexpr int TrinityForce                   = 3078;
    inline constexpr int RunaansHurricane               = 3085;
    inline constexpr int StatikkShiv                    = 3087;
    inline constexpr int RapidFirecannon                = 3094;
    inline constexpr int Stormrazor                     = 3095;
    inline constexpr int GuinsoosRageblade              = 3124;
    inline constexpr int YoumuusGhostblade              = 3142;
    inline constexpr int BladeOfTheRuinedKing           = 3153;
    inline constexpr int MawOfMalmortius                = 3156;
    inline constexpr int SpearOfShojin                  = 3161;
    inline constexpr int Hullbreaker                    = 3181;
    inline constexpr int Terminus                       = 3302;

    // ── AP legendaries ──
    inline constexpr int ArchangelsStaff                = 3003;
    inline constexpr int Manamune                       = 3004;
    inline constexpr int Muramana                       = 3042;
    inline constexpr int SeraphsEmbrace                 = 3040;
    inline constexpr int MejaisSoulstealer              = 3041;
    inline constexpr int RabadonsDeathcap               = 3089;
    inline constexpr int WitsEnd                        = 3091;
    inline constexpr int LichBane                       = 3100;
    inline constexpr int NashorsTooth                   = 3115;
    inline constexpr int RylaisCrystalScepter           = 3116;
    inline constexpr int HextechRocketbelt              = 3152;
    inline constexpr int ZhonyasHourglass               = 3157;
    inline constexpr int Morellonomicon                 = 3165;
    inline constexpr int VoidStaff                      = 3135;
    inline constexpr int HextechAlternator              = 3145;
    inline constexpr int MercurialScimitar              = 3139;

    // ── Tank / Bruiser / Support ──
    inline constexpr int IcebornGauntlet                = 3025;
    inline constexpr int GuardianAngel                  = 3026;
    inline constexpr int ZekesConvergence               = 3050;
    inline constexpr int SpiritVisage                   = 3065;
    inline constexpr int Kindlegem                      = 3067;
    inline constexpr int SunfireAegis                   = 3068;
    inline constexpr int Thornmail                      = 3075;
    inline constexpr int BramblesVest                   = 3076;
    inline constexpr int WardenMail                     = 3082;
    inline constexpr int WarmogsArmor                   = 3083;
    inline constexpr int Heartsteel                     = 3084;
    inline constexpr int BansheesVeil                   = 3102;
    inline constexpr int Redemption                     = 3107;
    inline constexpr int KnightsVow                     = 3109;
    inline constexpr int FrozenHeart                    = 3110;
    inline constexpr int RanduinsOmen                   = 3143;
    inline constexpr int LocketOfTheIronSolari          = 3190;
    inline constexpr int SeekersArmguard                = 3191;
    inline constexpr int GargoyleStoneplate             = 3193;
    inline constexpr int MikaelsBlessing                = 3222;

    inline constexpr int CaulfieldsWarhammer            = 3133;
    inline constexpr int SerratedDirk                   = 3134;
    inline constexpr int ExecutionersCalling            = 3123;
    inline constexpr int ChempunkChainsword             = 6609;

    // ── Jungle / Support "tome" line ──
    inline constexpr int SpellthiefsEdge                = 3850;
    inline constexpr int Frostfang                      = 3851;
    inline constexpr int ShardOfTrueIce                 = 3853;
    inline constexpr int RelicShield                    = 3858;
    inline constexpr int TargonsBuckler                 = 3859;
    inline constexpr int BulwarkOfTheMountain           = 3860;
    inline constexpr int Spectralsickle                 = 3862;
    inline constexpr int HarrowingCrescent              = 3863;
    inline constexpr int BlackMistScythe                = 3864;
    inline constexpr int Bloodsong                      = 3869;

    // ── Mythic / New (6xxx) ──
    inline constexpr int DeathsDance                    = 6333;
    inline constexpr int SunderedSky                    = 6610;
    inline constexpr int Stridebreaker                  = 6631;
    inline constexpr int DivineSunderer                 = 6632;
    inline constexpr int LiandrysAnguish                = 6653;
    inline constexpr int LudensCompanion                = 6655;
    inline constexpr int RodOfAges                      = 6657;
    inline constexpr int JakshoTheProtean               = 6665;
    inline constexpr int KrakenSlayer                   = 6672;
    inline constexpr int ImmortalShieldbow              = 6673;
    inline constexpr int NavoriFlickerblades            = 6675;
    inline constexpr int TheCollector                   = 6676;
    inline constexpr int Eclipse                        = 6692;
    inline constexpr int ProwlersClaw                   = 6693;
    inline constexpr int SeryldasGrudge                 = 6694;
    inline constexpr int SerpentsFang                   = 6695;
    inline constexpr int AxiomArc                       = 6696;
    inline constexpr int Hubris                         = 6697;
    inline constexpr int VoltaicCyclosword              = 6698;
    inline constexpr int Opportunity                    = 6701;

    // ── Trinkets ──
    inline constexpr int StealthWardTrinket             = 3340;
    inline constexpr int FarsightAlteration             = 3363;
    inline constexpr int OracleLens                     = 3364;

    // ======================================================================
    // Script-name -> Riot id lookup
    // ======================================================================
    //
    // Accepts the short CDragon/DDragon script id (e.g. "Tiamat", "TrinityForce",
    // "InfinityEdge"). Comparison is case-insensitive and ignores spaces and
    // apostrophes so "Rabadon's Deathcap" still resolves. Returns 0 on miss.

    struct NamedItemEntry {
        const char* name;
        int id;
    };

    inline const NamedItemEntry kNameTable[] = {
        // Consumables / wards
        { "HealthPotion",                   HealthPotion },
        { "TotalBiscuitOfEverlastingWill",  TotalBiscuitOfEverlastingWill },
        { "KircheisShard",                  KircheisShard },
        { "RefillablePotion",               RefillablePotion },
        { "CorruptingPotion",               CorruptingPotion },
        { "ControlWard",                    ControlWard },
        { "ElixirOfIron",                   ElixirOfIron },
        { "ElixirOfSorcery",                ElixirOfSorcery },
        { "ElixirOfWrath",                  ElixirOfWrath },

        // Starter / component
        { "BootsOfSpeed",                   BootsOfSpeed },
        { "FaerieCharm",                    FaerieCharm },
        { "RejuvenationBead",               RejuvenationBead },
        { "GiantsBelt",                     GiantsBelt },
        { "CloakOfAgility",                 CloakOfAgility },
        { "BlastingWand",                   BlastingWand },
        { "SapphireCrystal",                SapphireCrystal },
        { "RubyCrystal",                    RubyCrystal },
        { "ClothArmor",                     ClothArmor },
        { "ChainVest",                      ChainVest },
        { "NullMagicMantle",                NullMagicMantle },
        { "LongSword",                      LongSword },
        { "Pickaxe",                        Pickaxe },
        { "BFSword",                        BFSword },
        { "Dagger",                         Dagger },
        { "RecurveBow",                     RecurveBow },
        { "AmplifyingTome",                 AmplifyingTome },
        { "VampiricScepter",                VampiricScepter },
        { "DoransShield",                   DoransShield },
        { "DoransBlade",                    DoransBlade },
        { "DoransRing",                     DoransRing },
        { "NegatronCloak",                  NegatronCloak },
        { "NeedlesslyLargeRod",             NeedlesslyLargeRod },
        { "DarkSeal",                       DarkSeal },
        { "Cull",                           Cull },

        // Boots
        { "BerserkersGreaves",              BerserkersGreaves },
        { "BootsOfSwiftness",               BootsOfSwiftness },
        { "SorcerersShoes",                 SorcerersShoes },
        { "PlatedSteelcaps",                PlatedSteelcaps },
        { "MercurysTreads",                 MercurysTreads },
        { "IonianBootsOfLucidity",          IonianBootsOfLucidity },

        // AD / Crit / On-hit
        { "InfinityEdge",                   InfinityEdge },
        { "MortalReminder",                 MortalReminder },
        { "LastWhisper",                    LastWhisper },
        { "LordDominiksRegards",            LordDominiksRegards },
        { "PhantomDancer",                  PhantomDancer },
        { "SterakGage",                     SterakGage },
        { "SteraksGage",                    SterakGage },
        { "BlackCleaver",                   BlackCleaver },
        { "TheBlackCleaver",                BlackCleaver },
        { "Bloodthirster",                  Bloodthirster },
        { "TheBloodthirster",               Bloodthirster },
        { "RavenousHydra",                  RavenousHydra },
        { "Tiamat",                         Tiamat },
        { "TrinityForce",                   TrinityForce },
        { "RunaansHurricane",               RunaansHurricane },
        { "StatikkShiv",                    StatikkShiv },
        { "RapidFirecannon",                RapidFirecannon },
        { "Stormrazor",                     Stormrazor },
        { "GuinsoosRageblade",              GuinsoosRageblade },
        { "Rageblade",                      GuinsoosRageblade },
        { "YoumuusGhostblade",              YoumuusGhostblade },
        { "Ghostblade",                     YoumuusGhostblade },
        { "BladeOfTheRuinedKing",           BladeOfTheRuinedKing },
        { "BotRK",                          BladeOfTheRuinedKing },
        { "MawOfMalmortius",                MawOfMalmortius },
        { "SpearOfShojin",                  SpearOfShojin },
        { "Hullbreaker",                    Hullbreaker },
        { "Terminus",                       Terminus },

        // AP
        { "ArchangelsStaff",                ArchangelsStaff },
        { "Manamune",                       Manamune },
        { "Muramana",                       Muramana },
        { "SeraphsEmbrace",                 SeraphsEmbrace },
        { "MejaisSoulstealer",              MejaisSoulstealer },
        { "RabadonsDeathcap",               RabadonsDeathcap },
        { "Rabadons",                       RabadonsDeathcap },
        { "WitsEnd",                        WitsEnd },
        { "LichBane",                       LichBane },
        { "NashorsTooth",                   NashorsTooth },
        { "RylaisCrystalScepter",           RylaisCrystalScepter },
        { "Rylais",                         RylaisCrystalScepter },
        { "HextechRocketbelt",              HextechRocketbelt },
        { "Rocketbelt",                     HextechRocketbelt },
        { "ZhonyasHourglass",               ZhonyasHourglass },
        { "Zhonyas",                        ZhonyasHourglass },
        { "Morellonomicon",                 Morellonomicon },
        { "VoidStaff",                      VoidStaff },
        { "HextechAlternator",              HextechAlternator },
        { "MercurialScimitar",              MercurialScimitar },
        { "QuicksilverSash",                MercurialScimitar },

        // Tank / Bruiser / Support
        { "IcebornGauntlet",                IcebornGauntlet },
        { "GuardianAngel",                  GuardianAngel },
        { "ZekesConvergence",               ZekesConvergence },
        { "SpiritVisage",                   SpiritVisage },
        { "Kindlegem",                      Kindlegem },
        { "SunfireAegis",                   SunfireAegis },
        { "Thornmail",                      Thornmail },
        { "BramblesVest",                   BramblesVest },
        { "WardenMail",                     WardenMail },
        { "WarmogsArmor",                   WarmogsArmor },
        { "Warmogs",                        WarmogsArmor },
        { "Heartsteel",                     Heartsteel },
        { "BansheesVeil",                   BansheesVeil },
        { "Redemption",                     Redemption },
        { "KnightsVow",                     KnightsVow },
        { "FrozenHeart",                    FrozenHeart },
        { "RanduinsOmen",                   RanduinsOmen },
        { "Randuins",                       RanduinsOmen },
        { "LocketOfTheIronSolari",          LocketOfTheIronSolari },
        { "Locket",                         LocketOfTheIronSolari },
        { "SeekersArmguard",                SeekersArmguard },
        { "GargoyleStoneplate",             GargoyleStoneplate },
        { "MikaelsBlessing",                MikaelsBlessing },
        { "Mikaels",                        MikaelsBlessing },
        { "CaulfieldsWarhammer",            CaulfieldsWarhammer },
        { "SerratedDirk",                   SerratedDirk },
        { "ExecutionersCalling",            ExecutionersCalling },
        { "ChempunkChainsword",             ChempunkChainsword },

        // Jungle / Support tome
        { "SpellthiefsEdge",                SpellthiefsEdge },
        { "Frostfang",                      Frostfang },
        { "ShardOfTrueIce",                 ShardOfTrueIce },
        { "RelicShield",                    RelicShield },
        { "TargonsBuckler",                 TargonsBuckler },
        { "BulwarkOfTheMountain",           BulwarkOfTheMountain },
        { "Spectralsickle",                 Spectralsickle },
        { "HarrowingCrescent",              HarrowingCrescent },
        { "BlackMistScythe",                BlackMistScythe },
        { "Bloodsong",                      Bloodsong },

        // Mythic / new (6xxx)
        { "DeathsDance",                    DeathsDance },
        { "SunderedSky",                    SunderedSky },
        { "Stridebreaker",                  Stridebreaker },
        { "DivineSunderer",                 DivineSunderer },
        { "LiandrysAnguish",                LiandrysAnguish },
        { "Liandrys",                       LiandrysAnguish },
        { "LudensCompanion",                LudensCompanion },
        { "Ludens",                         LudensCompanion },
        { "RodOfAges",                      RodOfAges },
        { "JakshoTheProtean",               JakshoTheProtean },
        { "Jaksho",                         JakshoTheProtean },
        { "KrakenSlayer",                   KrakenSlayer },
        { "ImmortalShieldbow",              ImmortalShieldbow },
        { "Shieldbow",                      ImmortalShieldbow },
        { "NavoriFlickerblades",            NavoriFlickerblades },
        { "Navori",                         NavoriFlickerblades },
        { "TheCollector",                   TheCollector },
        { "Collector",                      TheCollector },
        { "Eclipse",                        Eclipse },
        { "ProwlersClaw",                   ProwlersClaw },
        { "Prowler",                        ProwlersClaw },
        { "SeryldasGrudge",                 SeryldasGrudge },
        { "Seryldas",                       SeryldasGrudge },
        { "SerpentsFang",                   SerpentsFang },
        { "AxiomArc",                       AxiomArc },
        { "Hubris",                         Hubris },
        { "VoltaicCyclosword",              VoltaicCyclosword },
        { "Opportunity",                    Opportunity },

        // Trinkets
        { "StealthWardTrinket",             StealthWardTrinket },
        { "FarsightAlteration",             FarsightAlteration },
        { "OracleLens",                     OracleLens },
    };

    // Case-insensitive char compare that also ignores spaces, underscores,
    // apostrophes, and dashes so "Rabadon's Deathcap" matches "RabadonsDeathcap".
    inline bool NameCharEqual(char a, char b) {
        auto norm = [](char c) -> char {
            if (c == ' ' || c == '_' || c == '\'' || c == '-') return 0;
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
            return c;
        };
        return norm(a) == norm(b);
    }

    inline bool NameEquals(const char* a, const char* b) {
        if (!a || !b) return false;
        while (*a || *b) {
            // Skip ignored characters on either side.
            while (*a && (*a == ' ' || *a == '_' || *a == '\'' || *a == '-')) ++a;
            while (*b && (*b == ' ' || *b == '_' || *b == '\'' || *b == '-')) ++b;
            if (!*a && !*b) return true;
            if (!*a || !*b) return false;
            if (!NameCharEqual(*a, *b)) return false;
            ++a; ++b;
        }
        return true;
    }

    inline int FromScript(const char* scriptName) {
        if (!scriptName || !*scriptName) return 0;
        for (const auto& e : kNameTable) {
            if (NameEquals(scriptName, e.name)) return e.id;
        }
        return 0;
    }

    inline const char* ToScript(int riotId) {
        if (riotId <= 0) return "";
        for (const auto& e : kNameTable) {
            if (e.id == riotId) return e.name;
        }
        return "";
    }

} // namespace Items
} // namespace SDK
