#pragma once
// ============================================================================
// AutoLevel — Automatically level up champion spells based on preset order
// ============================================================================
// Uses game function sub_BA39B0 (LevelSpell):
//   - rcx: don't care (overwritten internally with qword_1D80AA0)
//   - edx: spell slot index (0=Q, 1=W, 2=E, 3=R)
//
// Handlers found via IDA xrefs to evtLevelSpell1-4:
//   sub_B64890 (Q): xor edx,edx; call sub_BA39B0
//   sub_B64830 (W): mov edx,1;  call sub_BA39B0
//   sub_B64850 (E): mov edx,2;  call sub_BA39B0
//   sub_B64870 (R): mov edx,3;  call sub_BA39B0
// ============================================================================

#include "core/Offsets.h"
#include "core/Globals.h"
#include "GameObjects.h"
#include "SpellBook.h"
#include "MenuUI.h"
#include "Game.h"
#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")

extern "C" void* _spoofer_stub();
template <typename Ret, typename... Args>
static inline Ret spoof_call(void* trampoline, void* fn, Args... args);

namespace SDK {

    class AutoLevel {
    public:
        // ====================================================================
        // Preset level orders (18 entries, values 0-3 for Q/W/E/R)
        // ====================================================================
        // Standard presets: most common skill orders
        // Format: array of 18 ints, each = spell slot (0=Q, 1=W, 2=E, 3=R)
        
        // Generic presets by priority
        struct LevelPreset {
            const char* Name;
            int Order[18]; // spell slot for each level 1-18
        };

        // Generate order from priority string like "RQWE" (R first at 6/11/16, then max Q, then W, then E)
        static void GenerateOrder(const char* priority, int outOrder[18]) {
            // priority[0] = always R (ult), priority[1..3] = Q/W/E order
            // Standard LoL leveling rules:
            //   - R (ultimate) at levels 6, 11, 16
            //   - Other spells max at 5 each
            //   - At level 1, must pick one basic spell
            
            int slotMap[4] = { -1, -1, -1, -1 }; // priority index -> slot
            int slotPriority[3] = { 0, 1, 2 }; // basic spell priority (default Q>W>E)
            
            // Parse priority string (e.g., "QWE", "WEQ", "EQW")
            int pIdx = 0;
            for (int i = 0; priority[i] && pIdx < 3; i++) {
                char c = priority[i];
                if (c == 'Q' || c == 'q') slotPriority[pIdx++] = 0;
                else if (c == 'W' || c == 'w') slotPriority[pIdx++] = 1;
                else if (c == 'E' || c == 'e') slotPriority[pIdx++] = 2;
                // Skip 'R' in priority string, R is always at 6/11/16
            }
            
            int spellLevels[4] = { 0, 0, 0, 0 }; // current level of each spell
            int maxSpellLevel[4] = { 5, 5, 5, 3 }; // max levels for Q/W/E/R
            
            for (int lvl = 0; lvl < 18; lvl++) {
                int heroLevel = lvl + 1; // 1-based hero level
                
                // Check if we should level R (at levels 6, 11, 16)
                if ((heroLevel == 6 || heroLevel == 11 || heroLevel == 16) && spellLevels[3] < 3) {
                    outOrder[lvl] = 3; // R
                    spellLevels[3]++;
                    continue;
                }
                
                // Level basic spells by priority
                bool leveled = false;
                for (int p = 0; p < 3; p++) {
                    int slot = slotPriority[p];
                    if (spellLevels[slot] < maxSpellLevel[slot]) {
                        outOrder[lvl] = slot;
                        spellLevels[slot]++;
                        leveled = true;
                        break;
                    }
                }
                
                // Fallback: level any available spell
                if (!leveled) {
                    for (int s = 0; s < 4; s++) {
                        if (spellLevels[s] < maxSpellLevel[s]) {
                            outOrder[lvl] = s;
                            spellLevels[s]++;
                            break;
                        }
                    }
                }
            }
        }

        // ====================================================================
        // Init — register menu items
        // ====================================================================
        static void Init() {
            s_menu = SDK::MenuUI::Menu::Create("AutoLevel", "Auto Level");
            s_menu->Add<SDK::MenuUI::MenuBool>("Enabled", "Enable Auto Level", false);
            
            // Level order combo: QWE, QEW, WQE, WEQ, EQW, EWQ
            s_menu->Add<SDK::MenuUI::MenuList>("Priority", "Skill Priority",
                std::vector<std::string>{ 
                    "Q > W > E",   // 0
                    "Q > E > W",   // 1
                    "W > Q > E",   // 2
                    "W > E > Q",   // 3
                    "E > Q > W",   // 4
                    "E > W > Q"    // 5
                }, 0);
            
            // First skill at level 1
            s_menu->Add<SDK::MenuUI::MenuList>("FirstSkill", "First Skill (Lv1)",
                std::vector<std::string>{ "Q", "W", "E" }, 0);
            
            // Delay between level-ups (to look natural)
            s_menu->Add<SDK::MenuUI::MenuSlider>("Delay", "Level-Up Delay (ms)", 300, 50, 2000);
            
            // Only auto-level after reaching a certain hero level
            s_menu->Add<SDK::MenuUI::MenuSlider>("MinLevel", "Start Auto Level At", 1, 1, 18);
            
            // Generate default order
            GenerateOrderFromMenu();
            
            s_initialized = true;
        }

        // ====================================================================
        // Update — called each frame, checks for available skill points
        // ====================================================================
        static void Update() {
            if (!s_initialized) return;
            
            if (!s_menu) return;
            
            auto* enabled = s_menu->Get<SDK::MenuUI::MenuBool>("Enabled");
            if (!enabled || !enabled->Enabled) return;
            
            auto& player = GameObjects::Player;
            if (!player.IsValid() || !player.IsAlive()) return;
            
            int heroLevel = player.GetLevel();
            if (heroLevel <= 0 || heroLevel > 18) return;
            
            // Check minimum level
            auto* minLevel = s_menu->Get<SDK::MenuUI::MenuSlider>("MinLevel");
            if (minLevel && heroLevel < minLevel->Value) return;
            
            // Check delay
            auto* delay = s_menu->Get<SDK::MenuUI::MenuSlider>("Delay");
            float delayMs = delay ? (float)delay->Value : 300.0f;
            float now = Game::GetTime() * 1000.0f; // convert to ms
            if (now - s_lastLevelTime < delayMs) return;
            
            // Check if priority changed
            auto* priority = s_menu->Get<SDK::MenuUI::MenuList>("Priority");
            auto* firstSkill = s_menu->Get<SDK::MenuUI::MenuList>("FirstSkill");
            int currentPri = priority ? priority->Index : 0;
            int currentFirst = firstSkill ? firstSkill->Index : 0;
            if (currentPri != s_lastPriority || currentFirst != s_lastFirstSkill) {
                s_lastPriority = currentPri;
                s_lastFirstSkill = currentFirst;
                GenerateOrderFromMenu();
            }
            
            // Get available skill points
            int availablePoints = GetAvailableSkillPoints(player, heroLevel);
            if (availablePoints <= 0) return;
            
            // Determine which spell to level based on order
            // We need to figure out the "next" spell to level
            // Count total spell levels already assigned
            SpellBook sb(player.address);
            int currentSpellLevels[4] = { 0, 0, 0, 0 };
            for (int i = 0; i < 4; i++) {
                auto spell = sb.GetSpell((SpellSlotId)i);
                currentSpellLevels[i] = spell.IsValid() ? spell.GetLevel() : 0;
            }
            
            int totalLeveled = currentSpellLevels[0] + currentSpellLevels[1] + 
                               currentSpellLevels[2] + currentSpellLevels[3];
            
            // The next spell to level is s_levelOrder[totalLeveled]
            if (totalLeveled < 0 || totalLeveled >= 18) return;
            
            int slotToLevel = s_levelOrder[totalLeveled];
            if (slotToLevel < 0 || slotToLevel > 3) return;
            
            // Validate: can this spell actually be leveled?
            int maxLevel = (slotToLevel == 3) ? 3 : 5;
            if (currentSpellLevels[slotToLevel] >= maxLevel) {
                // Try to find an alternative
                slotToLevel = FindAlternativeSlot(currentSpellLevels, heroLevel);
                if (slotToLevel < 0) return;
            }
            
            // Additional validation for R: can only level at 6, 11, 16
            if (slotToLevel == 3) {
                int rLevel = currentSpellLevels[3];
                bool canLevelR = (rLevel == 0 && heroLevel >= 6) ||
                                 (rLevel == 1 && heroLevel >= 11) ||
                                 (rLevel == 2 && heroLevel >= 16);
                if (!canLevelR) {
                    slotToLevel = FindAlternativeSlot(currentSpellLevels, heroLevel, true);
                    if (slotToLevel < 0) return;
                }
            }
            
            // Execute level-up!
            LevelUpSpell(slotToLevel);
            s_lastLevelTime = now;
        }

    private:
        static inline bool s_initialized = false;
        static inline std::shared_ptr<SDK::MenuUI::Menu> s_menu;
        static inline int  s_levelOrder[18] = { 0 };
        static inline float s_lastLevelTime = 0.0f;
        static inline int  s_lastPriority = -1;
        static inline int  s_lastFirstSkill = -1;
        
        // ====================================================================
        // Get available skill points
        // ====================================================================
        static int GetAvailableSkillPoints(const GameObject& player, int heroLevel) {
            // Method 1: Read LevelUpPoints offset directly
            int points = Globals::Read<int>(player.address + Offset::Hero::LevelUpPoints);
            if (points > 0 && points <= 18) return points;
            
            // Method 2: Calculate from hero level minus total spell levels
            SpellBook sb(player.address);
            int totalSpellLevels = 0;
            for (int i = 0; i < 4; i++) {
                auto spell = sb.GetSpell((SpellSlotId)i);
                if (spell.IsValid()) totalSpellLevels += spell.GetLevel();
            }
            
            int calculated = heroLevel - totalSpellLevels;
            return (calculated > 0) ? calculated : 0;
        }
        
        // ====================================================================
        // Generate level order from menu settings
        // ====================================================================
        static void GenerateOrderFromMenu() {
            if (!s_menu) return;
            
            auto* priority = s_menu->Get<SDK::MenuUI::MenuList>("Priority");
            auto* firstSkill = s_menu->Get<SDK::MenuUI::MenuList>("FirstSkill");
            
            int priIdx = priority ? priority->Index : 0;
            int firstIdx = firstSkill ? firstSkill->Index : 0;
            
            const char* priorities[] = {
                "QWE", "QEW", "WQE", "WEQ", "EQW", "EWQ"
            };
            
            const char* pri = (priIdx >= 0 && priIdx < 6) ? priorities[priIdx] : "QWE";
            
            GenerateOrder(pri, s_levelOrder);
            
            // Override first skill
            if (firstIdx >= 0 && firstIdx <= 2) {
                s_levelOrder[0] = firstIdx;
                
                // Regenerate rest to account for the change
                // Simple approach: just set level 1, regenerate levels 2-18
                int spellLevels[4] = { 0, 0, 0, 0 };
                spellLevels[firstIdx] = 1;
                
                int slotPriority[3] = { 0, 1, 2 };
                int pIdx = 0;
                for (int i = 0; pri[i] && pIdx < 3; i++) {
                    char c = pri[i];
                    if (c == 'Q' || c == 'q') slotPriority[pIdx++] = 0;
                    else if (c == 'W' || c == 'w') slotPriority[pIdx++] = 1;
                    else if (c == 'E' || c == 'e') slotPriority[pIdx++] = 2;
                }
                
                int maxSpellLevel[4] = { 5, 5, 5, 3 };
                
                for (int lvl = 1; lvl < 18; lvl++) {
                    int heroLevel = lvl + 1;
                    
                    if ((heroLevel == 6 || heroLevel == 11 || heroLevel == 16) && spellLevels[3] < 3) {
                        s_levelOrder[lvl] = 3;
                        spellLevels[3]++;
                        continue;
                    }
                    
                    bool leveled = false;
                    for (int p = 0; p < 3; p++) {
                        int slot = slotPriority[p];
                        if (spellLevels[slot] < maxSpellLevel[slot]) {
                            s_levelOrder[lvl] = slot;
                            spellLevels[slot]++;
                            leveled = true;
                            break;
                        }
                    }
                    
                    if (!leveled) {
                        for (int s = 0; s < 4; s++) {
                            if (spellLevels[s] < maxSpellLevel[s]) {
                                s_levelOrder[lvl] = s;
                                spellLevels[s]++;
                                break;
                            }
                        }
                    }
                }
            }
        }
        
        // ====================================================================
        // Find alternative slot when preferred slot can't be leveled
        // ====================================================================
        static int FindAlternativeSlot(int currentLevels[4], int heroLevel, bool skipR = false) {
            // Try to level based on priority order
            auto* priority = s_menu ? s_menu->Get<SDK::MenuUI::MenuList>("Priority") : nullptr;
            int priIdx = priority ? priority->Index : 0;
            
            const char* priorities[] = { "QWE", "QEW", "WQE", "WEQ", "EQW", "EWQ" };
            const char* pri = (priIdx >= 0 && priIdx < 6) ? priorities[priIdx] : "QWE";
            
            int slotPriority[3] = { 0, 1, 2 };
            int pIdx = 0;
            for (int i = 0; pri[i] && pIdx < 3; i++) {
                char c = pri[i];
                if (c == 'Q' || c == 'q') slotPriority[pIdx++] = 0;
                else if (c == 'W' || c == 'w') slotPriority[pIdx++] = 1;
                else if (c == 'E' || c == 'e') slotPriority[pIdx++] = 2;
            }
            
            // Try R first (if not skipping)
            if (!skipR && currentLevels[3] < 3) {
                int rLevel = currentLevels[3];
                bool canLevelR = (rLevel == 0 && heroLevel >= 6) ||
                                 (rLevel == 1 && heroLevel >= 11) ||
                                 (rLevel == 2 && heroLevel >= 16);
                if (canLevelR) return 3;
            }
            
            // Try basic spells by priority
            for (int p = 0; p < 3; p++) {
                int slot = slotPriority[p];
                if (currentLevels[slot] < 5) {
                    return slot;
                }
            }
            
            return -1; // no slot available
        }

        // ====================================================================
        // Find trampoline gadget for spoof_call (reuse from SpellCaster)
        // ====================================================================
        static void* GetTrampoline() {
            static void* trampoline = nullptr;
            if (!trampoline) {
                MODULEINFO modInfo{};
                GetModuleInformation(GetCurrentProcess(),
                    GetModuleHandleA("League of Legends.exe"),
                    &modInfo, sizeof(modInfo));
                
                BYTE* base = (BYTE*)modInfo.lpBaseOfDll;
                DWORD size = modInfo.SizeOfImage;
                
                // Search for FF 23 (jmp [rbx]) gadget
                for (DWORD i = 0; i < size - 1; i++) {
                    if (base[i] == 0xFF && base[i + 1] == 0x23) {
                        trampoline = &base[i];
                        break;
                    }
                }
            }
            return trampoline;
        }

        // ====================================================================
        // Call LevelSpell game function
        // ====================================================================
        // sub_BA39B0: __fastcall(void* dummy, int slotIdx)
        //   - rcx (dummy): immediately overwritten with qword_1D80AA0
        //   - edx (slotIdx): 0=Q, 1=W, 2=E, 3=R
        // ====================================================================
        static void LevelUpSpell(int slotIdx) {
            if (slotIdx < 0 || slotIdx > 3) return;
            
            void* trampoline = GetTrampoline();
            if (!trampoline) return;
            
            using fnLevelSpell = void(__fastcall*)(void*, int);
            fnLevelSpell fn = reinterpret_cast<fnLevelSpell>(
                Globals::base + Offset::Function::LevelSpell);
            
            __try {
                spoof_call(trampoline, fn, (void*)0, slotIdx);
            } __except(1) {
                // LevelSpell crashed — ignore
            }
        }
    };

} // namespace SDK
