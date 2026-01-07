#pragma once
#include <map>
#include <string>
#include <vector>

namespace SDK
{
    // ============================================================================
    // Champion Priority Database - All 174 Champions (December 2024)
    // Priority: 5 = Max (ADC/Mage Carry), 4 = High (Assassin), 3 = Medium (Fighter), 2 = Default, 1 = Low (Tank)
    // Based on NewTargetSelector.cs GetDefaultPriority()
    // ============================================================================
    class ChampionDatabase
    {
    public:
        struct ChampionInfo {
            std::string Name;
            int DefaultPriority;
        };

        static int GetPriority(const std::string& championName) {
            auto& priorities = GetRuntimePriorities();
            auto it = priorities.find(championName);
            if (it != priorities.end()) {
                return it->second;
            }
            return GetDefaultPriority(championName);
        }

        static void SetPriority(const std::string& championName, int priority) {
            GetRuntimePriorities()[championName] = priority;
        }

        // Get default priority (based on champion class)
        static int GetDefaultPriority(const std::string& name) {
            // Priority 5 - Max Priority (ADC, Burst Mages, Squishy Carries)
            if (name == "Aphelios" || name == "Ashe" || name == "Caitlyn" || name == "Corki" ||
                name == "Draven" || name == "Ezreal" || name == "Jhin" || name == "Jinx" ||
                name == "Kaisa" || name == "Kalista" || name == "KogMaw" || name == "Lucian" ||
                name == "MissFortune" || name == "Nilah" || name == "Quinn" || name == "Samira" ||
                name == "Sivir" || name == "Smolder" || name == "Tristana" || name == "Twitch" ||
                name == "Varus" || name == "Vayne" || name == "Xayah" || name == "Zeri" ||
                // Mages
                name == "Ahri" || name == "Anivia" || name == "Annie" || name == "AurelionSol" ||
                name == "Azir" || name == "Brand" || name == "Cassiopeia" || name == "Hwei" ||
                name == "Karma" || name == "Karthus" || name == "Katarina" || name == "Kennen" ||
                name == "Leblanc" || name == "Lux" || name == "Malzahar" || name == "Neeko" ||
                name == "Orianna" || name == "Seraphine" || name == "Syndra" || name == "Taliyah" ||
                name == "Teemo" || name == "TwistedFate" || name == "Veigar" || name == "Velkoz" ||
                name == "Vex" || name == "Viktor" || name == "Xerath" || name == "Ziggs" || name == "Zoe" ||
                // Squishy Assassins
                name == "Akshan" || name == "Naafiri" || name == "Qiyana" || name == "Talon" || name == "Zed" ||
                // Enchanters
                name == "Janna" || name == "Lulu" || name == "Milio" || name == "Nami" ||
                name == "Sona" || name == "Soraka" || name == "Yuumi") {
                return 5;
            }
            
            // Priority 4 - High Priority (Assassins, Skirmishers)
            if (name == "Akali" || name == "Belveth" || name == "Briar" || name == "Camille" ||
                name == "Diana" || name == "Ekko" || name == "Evelynn" || name == "Fiddlesticks" ||
                name == "Fiora" || name == "Fizz" || name == "Gwen" || name == "Heimerdinger" ||
                name == "Jayce" || name == "Kassadin" || name == "Kayle" || name == "Kayn" ||
                name == "Khazix" || name == "Kindred" || name == "Lissandra" || name == "MasterYi" ||
                name == "Mordekaiser" || name == "Nidalee" || name == "Pyke" || name == "Rengar" ||
                name == "Riven" || name == "Senna" || name == "Shaco" || name == "Sylas" ||
                name == "Viego" || name == "Vladimir" || name == "Yasuo" || name == "Yone" || name == "Zilean") {
                return 4;
            }
            
            // Priority 3 - Medium Priority (Bruisers, Fighters, Divers)
            if (name == "Aatrox" || name == "Ambessa" || name == "Aurora" || name == "Darius" ||
                name == "Elise" || name == "Galio" || name == "Gangplank" || name == "Gragas" ||
                name == "Graves" || name == "Hecarim" || name == "Illaoi" || name == "Irelia" ||
                name == "Jax" || name == "JarvanIV" || name == "Kled" || name == "KSante" ||
                name == "LeeSin" || name == "Lillia" || name == "Maokai" || name == "Mel" ||
                name == "Morgana" || name == "Nocturne" || name == "Olaf" || name == "Pantheon" ||
                name == "Poppy" || name == "RekSai" || name == "Renekton" || name == "Rumble" ||
                name == "Ryze" || name == "Sett" || name == "Shyvana" || name == "Swain" ||
                name == "Trundle" || name == "Tryndamere" || name == "Udyr" || name == "Urgot" ||
                name == "Vi" || name == "Volibear" || name == "Warwick" || name == "MonkeyKing" ||
                name == "XinZhao" || name == "Yorick" || name == "Zyra") {
                return 3;
            }
            
            // Priority 1 - Low Priority (Tanks, Wardens, Tank Supports)
            if (name == "Alistar" || name == "Amumu" || name == "Bard" || name == "Blitzcrank" ||
                name == "Braum" || name == "Chogath" || name == "DrMundo" || name == "Garen" ||
                name == "Gnar" || name == "Ivern" || name == "Leona" || name == "Malphite" ||
                name == "Nasus" || name == "Nautilus" || name == "Nunu" || name == "Ornn" ||
                name == "Rammus" || name == "Rell" || name == "Sejuani" || name == "Shen" ||
                name == "Singed" || name == "Sion" || name == "Skarner" || name == "TahmKench" ||
                name == "Taric" || name == "Thresh" || name == "Zac" || name == "Rakan" || name == "Renata") {
                return 1;
            }
            
            // New champions (default priority until role is determined)
            if (name == "Yunara" || name == "Zaahen") {
                return 2; // Default priority
            }
            
            // Default - Priority 2
            return 2;
        }

        // Get list of all 174 champions (for menu display)
        static const std::vector<std::string>& GetAllChampions() {
            static std::vector<std::string> champions = {
                // A
                "Aatrox", "Ahri", "Akali", "Akshan", "Alistar", "Ambessa", "Amumu", "Anivia", 
                "Annie", "Aphelios", "Ashe", "AurelionSol", "Aurora", "Azir",
                // B
                "Bard", "Belveth", "Blitzcrank", "Brand", "Braum", "Briar",
                // C
                "Caitlyn", "Camille", "Cassiopeia", "Chogath", "Corki",
                // D
                "Darius", "Diana", "DrMundo", "Draven",
                // E
                "Ekko", "Elise", "Evelynn", "Ezreal",
                // F
                "Fiddlesticks", "Fiora", "Fizz",
                // G
                "Galio", "Gangplank", "Garen", "Gnar", "Gragas", "Graves", "Gwen",
                // H
                "Hecarim", "Heimerdinger", "Hwei",
                // I
                "Illaoi", "Irelia", "Ivern",
                // J
                "Janna", "JarvanIV", "Jax", "Jayce", "Jhin", "Jinx",
                // K
                "Kaisa", "Kalista", "Karma", "Karthus", "Kassadin", "Katarina", "Kayle",
                "Kayn", "Kennen", "Khazix", "Kindred", "Kled", "KogMaw", "KSante",
                // L
                "Leblanc", "LeeSin", "Leona", "Lillia", "Lissandra", "Lucian", "Lulu", "Lux",
                // M
                "Malphite", "Malzahar", "Maokai", "MasterYi", "Mel", "Milio", "MissFortune", 
                "MonkeyKing", "Mordekaiser", "Morgana",
                // N
                "Naafiri", "Nami", "Nasus", "Nautilus", "Neeko", "Nidalee", "Nilah", 
                "Nocturne", "Nunu",
                // O
                "Olaf", "Orianna", "Ornn",
                // P
                "Pantheon", "Poppy", "Pyke",
                // Q
                "Qiyana", "Quinn",
                // R
                "Rakan", "Rammus", "RekSai", "Rell", "Renata", "Renekton", "Rengar", 
                "Riven", "Rumble", "Ryze",
                // S
                "Samira", "Sejuani", "Senna", "Seraphine", "Sett", "Shaco", "Shen", 
                "Shyvana", "Singed", "Sion", "Sivir", "Skarner", "Smolder", "Sona", 
                "Soraka", "Swain", "Sylas", "Syndra",
                // T
                "TahmKench", "Taliyah", "Talon", "Taric", "Teemo", "Thresh", "Tristana", 
                "Trundle", "Tryndamere", "TwistedFate", "Twitch",
                // U
                "Udyr", "Urgot",
                // V
                "Varus", "Vayne", "Veigar", "Velkoz", "Vex", "Vi", "Viego", "Viktor", 
                "Vladimir", "Volibear",
                // W
                "Warwick",
                // X
                "Xayah", "Xerath", "XinZhao",
                // Y
                "Yasuo", "Yone", "Yorick", "Yuumi", "Yunara",
                // Z
                "Zac", "Zed", "Zeri", "Ziggs", "Zilean", "Zoe", "Zaahen", "Zyra"
            };
            return champions;
        }

        static std::map<std::string, int>& GetRuntimePriorities() {
            static std::map<std::string, int> priorities;
            return priorities;
        }
    };
}
