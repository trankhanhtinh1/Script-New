#pragma once

#include "../IPlugin.h"
#include "../../core/CoreObjects.h"
#include "../../core/CoreSpellBook.h"
#include "../../core/CoreSpellCastInfo.h"
#include "../../core/Globals.h"
#include "../../core/Offsets.h"
#include "../../menu/MenuUI.h"

#include <cstdio>
#include <cstring>

namespace Plugins {

using namespace SDK::MenuUI;

class OffsetScannerPlugin : public IPlugin {
public:
    const char* GetName() const override { return "Offset Scanner"; }
    const char* GetInternalId() const override { return "ns.offset_scanner"; }
    PluginCategory GetCategory() const override { return PluginCategory::Utility; }
    bool AutoLoadByDefault() const override { return true; }

    void OnLoad() override {
        if (m_menu) return;
        m_menu = Menu::Create("OffsetScannerRoot", "[NightSharp] Offset Scanner");
        m_menu->Add<MenuBool>("RunScan", "Run Scan (toggle ON to scan)", false);
        m_menu->Add<MenuSeparator>("sep1", "Results saved to C:\\Users\\Public\\ns_offset_scan.txt");
    }

    void OnUnload() override {
        if (!m_menu) return;
        Menu::Remove("OffsetScannerRoot");
        m_menu = nullptr;
    }

    Menu* GetMenuRoot() override { return m_menu; }

    void OnUpdate() override {
        if (!m_menu) return;
        auto* scanToggle = m_menu->Get<MenuBool>("RunScan");
        if (scanToggle && scanToggle->Enabled) {
            scanToggle->Enabled = false; // auto reset
            RunFullScan();
        }
    }

private:
    Menu* m_menu = nullptr;
    bool m_scanDone = false;
    char m_lastStatus[128] = "Ready";
    char m_preview[4096] = {};

    // ────────────────────────────────────────────────────
    // Try reading a string at (base + offset) using multiple strategies
    // ────────────────────────────────────────────────────
    static bool TryReadString(uintptr_t addr, char* out, int maxOut) {
        if (!Globals::IsValidPtr(addr)) return false;
        out[0] = 0;
        if (Globals::ReadStdString(addr, out, maxOut) && out[0]) return true;
        if (Globals::ReadRiotString(addr, out, maxOut) && out[0]) return true;
        if (Globals::ReadCString(addr, out, maxOut) && out[0]) return true;
        // Try pointer-to-cstring
        uintptr_t ptr = Globals::Read<uintptr_t>(addr);
        if (Globals::IsValidPtr(ptr) && Globals::ReadCString(ptr, out, maxOut) && out[0]) return true;
        return false;
    }

    // ────────────────────────────────────────────────────
    // Scan a range of offsets for a valid string
    // ────────────────────────────────────────────────────
    struct ScanHit {
        int offset;
        char value[128];
        char method[32];
    };

    static int ScanStringField(uintptr_t objBase, int centerOffset, int range, ScanHit* hits, int maxHits) {
        int count = 0;
        const int step = 4;
        for (int delta = -range; delta <= range && count < maxHits; delta += step) {
            const int off = centerOffset + delta;
            if (off < 0) continue;
            uintptr_t addr = objBase + off;
            char buf[128] = {};

            // Strategy 1: std::string SSO
            if (Globals::ReadStdString(addr, buf, sizeof(buf)) && buf[0] && IsLikelyName(buf)) {
                auto& h = hits[count++];
                h.offset = off;
                strncpy_s(h.value, buf, _TRUNCATE);
                strncpy_s(h.method, "StdString", _TRUNCATE);
                continue;
            }

            // Strategy 2: RiotString
            if (Globals::ReadRiotString(addr, buf, sizeof(buf)) && buf[0] && IsLikelyName(buf)) {
                auto& h = hits[count++];
                h.offset = off;
                strncpy_s(h.value, buf, _TRUNCATE);
                strncpy_s(h.method, "RiotString", _TRUNCATE);
                continue;
            }

            // Strategy 3: pointer to CString
            uintptr_t ptr = Globals::Read<uintptr_t>(addr);
            if (Globals::IsValidPtr(ptr) && Globals::ReadCString(ptr, buf, sizeof(buf)) && buf[0] && IsLikelyName(buf)) {
                auto& h = hits[count++];
                h.offset = off;
                strncpy_s(h.value, buf, _TRUNCATE);
                strncpy_s(h.method, "Ptr->CStr", _TRUNCATE);
                continue;
            }

            // Strategy 4: direct inline CString
            if (Globals::ReadCString(addr, buf, sizeof(buf)) && buf[0] && strlen(buf) >= 3 && IsLikelyName(buf)) {
                auto& h = hits[count++];
                h.offset = off;
                strncpy_s(h.value, buf, _TRUNCATE);
                strncpy_s(h.method, "InlineCStr", _TRUNCATE);
            }
        }
        return count;
    }

    static bool IsLikelyName(const char* s) {
        if (!s || !s[0]) return false;
        int len = 0;
        for (int i = 0; s[i] && i < 127; ++i) {
            unsigned char c = (unsigned char)s[i];
            if (c < 0x20 || c > 0x7E) return false;
            len++;
        }
        return len >= 2 && len <= 80;
    }

    // ────────────────────────────────────────────────────
    // Scan SpellData → SpellName offset
    // ────────────────────────────────────────────────────
    struct SpellDataScanResult {
        int spellNameOffset;
        char spellName[128];
        char method[32];
        bool found;
    };

    static SpellDataScanResult ScanSpellDataName(uintptr_t spellDataAddr, int centerOffset, int range) {
        SpellDataScanResult result = {};
        ScanHit hits[32] = {};
        int count = ScanStringField(spellDataAddr, centerOffset, range, hits, 32);
        if (count > 0) {
            result.found = true;
            result.spellNameOffset = hits[0].offset;
            strncpy_s(result.spellName, hits[0].value, _TRUNCATE);
            strncpy_s(result.method, hits[0].method, _TRUNCATE);
        }
        return result;
    }

    // ────────────────────────────────────────────────────
    // Main scan routine
    // ────────────────────────────────────────────────────
    void RunFullScan() {
        const auto local = CoreObjects::GetLocalPlayer();
        if (!local.IsValid()) {
            snprintf(m_lastStatus, sizeof(m_lastStatus), "FAIL: no local player");
            return;
        }

        HANDLE hFile = CreateFileA(
            "C:\\Users\\Public\\ns_offset_scan.txt",
            GENERIC_WRITE, FILE_SHARE_READ, nullptr,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (hFile == INVALID_HANDLE_VALUE) {
            snprintf(m_lastStatus, sizeof(m_lastStatus), "FAIL: cannot create output file");
            return;
        }

        auto W = [&](const char* fmt, ...) {
            char line[512];
            va_list args;
            va_start(args, fmt);
            int len = vsnprintf(line, sizeof(line), fmt, args);
            va_end(args);
            if (len > 0) {
                DWORD written;
                WriteFile(hFile, line, (DWORD)len, &written, nullptr);
            }
        };

        const uintptr_t obj = local.address;
        const uintptr_t base = Globals::base;

        W("=== NightSharp Offset Scanner ===\r\n");
        W("Module Base: 0x%llX\r\n", (unsigned long long)base);
        W("LocalPlayer: 0x%llX\r\n", (unsigned long long)obj);
        W("\r\n");

        // ── 1. GameObject::Name (current: 0x58) ──
        W("--- GameObject::Name (current: 0x%X) ---\r\n", Offset::All::Name);
        {
            ScanHit hits[64] = {};
            int count = ScanStringField(obj, Offset::All::Name, 0x80, hits, 64);
            W("  Found %d candidates:\r\n", count);
            for (int i = 0; i < count; ++i) {
                W("    [0x%04X] %-12s = \"%s\"%s\r\n",
                    hits[i].offset, hits[i].method, hits[i].value,
                    hits[i].offset == Offset::All::Name ? " <-- CURRENT" : "");
            }
        }

        // ── 2. GameObject::CharacterName (current: 0x4328) ──
        W("\r\n--- GameObject::CharacterName (current: 0x%X) ---\r\n", Offset::All::CharacterName);
        {
            ScanHit hits[64] = {};
            int count = ScanStringField(obj, Offset::All::CharacterName, 0x80, hits, 64);
            W("  Found %d candidates:\r\n", count);
            for (int i = 0; i < count; ++i) {
                W("    [0x%04X] %-12s = \"%s\"%s\r\n",
                    hits[i].offset, hits[i].method, hits[i].value,
                    hits[i].offset == Offset::All::CharacterName ? " <-- CURRENT" : "");
            }
        }

        // ── 3. ALL Slots dump with 26.7 chain ──
        W("\r\n--- All Spell Slots (26.7 chain: slot+0x130 → +0x60 → SpellData+0x80) ---\r\n");
        {
            const uintptr_t spellBook = obj + Offset::SpellBook::Offset;
            W("  SpellBook = obj+0x%X = 0x%llX\r\n", Offset::SpellBook::Offset, (unsigned long long)spellBook);

            for (int si = 0; si < 16; ++si) {
                const uintptr_t slot = Globals::Read<uintptr_t>(spellBook + Offset::SpellBook::SpellSlotArray + si * 8);
                if (!Globals::IsValidPtr(slot)) {
                    W("  Slot[%2d]: INVALID\r\n", si);
                    continue;
                }
                // 26.7 chain: slot+0x130 → intermediate → +0x60 → SpellData
                const uintptr_t inter = Globals::Read<uintptr_t>(slot + 0x130);
                uintptr_t spellData = 0;
                char name[64] = {};
                if (Globals::IsValidPtr(inter)) {
                    spellData = Globals::Read<uintptr_t>(inter + 0x60);
                    if (Globals::IsValidPtr(spellData)) {
                        TryReadString(spellData + 0x80, name, sizeof(name));
                    }
                }
                // Also try old chain as fallback
                char nameOld[64] = {};
                const uintptr_t info128 = Globals::Read<uintptr_t>(slot + 0x128);
                if (Globals::IsValidPtr(info128)) {
                    const uintptr_t sd = Globals::Read<uintptr_t>(info128 + 0x60);
                    if (Globals::IsValidPtr(sd)) {
                        TryReadString(sd + 0x80, nameOld, sizeof(nameOld));
                    }
                }
                W("  Slot[%2d]: ptr=0x%llX inter130=0x%llX sd=0x%llX name=\"%s\" old_chain=\"%s\"\r\n",
                    si, (unsigned long long)slot,
                    (unsigned long long)inter,
                    (unsigned long long)spellData,
                    name[0] ? name : "<empty>",
                    nameOld[0] ? nameOld : "<empty>");
            }

            // Direct GetSlot+GetSpellData test (what ReadSpellName actually does)
            W("\r\n  [ReadSpellName actual result for slots 0-3]:\r\n");
            for (int si = 0; si < 4; ++si) {
                auto slotRef = CoreAPI::SpellBook::GetSlot(obj, si);
                char buf[64] = {};
                slotRef.ReadSpellName(buf, sizeof(buf));
                W("    GetSlot(%d).ReadSpellName = \"%s\" (slotAddr=0x%llX)\r\n",
                    si, buf[0] ? buf : "<empty>", (unsigned long long)slotRef.address);
            }

            const uintptr_t slot0 = Globals::Read<uintptr_t>(spellBook + Offset::SpellBook::SpellSlotArray);

            if (Globals::IsValidPtr(slot0)) {
                // GetSpellInfo: Read(slot + SlotSpellInfo)
                const uintptr_t spellInfo = Globals::Read<uintptr_t>(slot0 + Offset::SpellBook::SlotSpellInfo);
                W("  SpellInfo = Read(Slot+0x%X) = 0x%llX %s\r\n",
                    Offset::SpellBook::SlotSpellInfo, (unsigned long long)spellInfo,
                    Globals::IsValidPtr(spellInfo) ? "VALID" : "INVALID");

                // GetSpellData: Read(SpellInfo + InfoSpellData)
                if (Globals::IsValidPtr(spellInfo)) {
                    const uintptr_t spellData = Globals::Read<uintptr_t>(spellInfo + Offset::SpellBook::InfoSpellData);
                    W("  SpellData = Read(SpellInfo+0x%X) = 0x%llX %s\r\n",
                        Offset::SpellBook::InfoSpellData, (unsigned long long)spellData,
                        Globals::IsValidPtr(spellData) ? "VALID" : "INVALID");

                    // Try reading SpellName from BOTH SpellInfo and SpellData
                    char nameA[64] = {}, nameB[64] = {};
                    Globals::ReadRuntimeStringField(spellInfo + Offset::SpellBook::DataSpellName, nameA, sizeof(nameA));
                    W("  SpellInfo+0x%X (name): \"%s\"\r\n", Offset::SpellBook::DataSpellName, nameA[0] ? nameA : "<empty>");

                    if (Globals::IsValidPtr(spellData)) {
                        Globals::ReadRuntimeStringField(spellData + Offset::SpellBook::DataSpellName, nameB, sizeof(nameB));
                        W("  SpellData+0x%X (name): \"%s\"\r\n", Offset::SpellBook::DataSpellName, nameB[0] ? nameB : "<empty>");
                    }

                    // Brute scan SpellInfo for any strings
                    W("\r\n  [SpellInfo strings scan 0x0-0x100]:\r\n");
                    ScanHit hits[32] = {};
                    int count = ScanStringField(spellInfo, 0x40, 0xC0, hits, 32);
                    for (int i = 0; i < count; ++i) {
                        W("    SpellInfo[+0x%04X] %-12s = \"%s\"\r\n",
                            hits[i].offset, hits[i].method, hits[i].value);
                    }

                    // Find InfoSpellData: scan SpellInfo ptrs → try SpellData+0x80 (DataSpellName)
                    W("\r\n  [Find InfoSpellData: SpellInfo+X → ptr → +0x80 = name?]\r\n");
                    for (int off = 0; off <= 0x100; off += 8) {
                        uintptr_t p = Globals::Read<uintptr_t>(spellInfo + off);
                        if (!Globals::IsValidPtr(p)) continue;
                        char buf[64] = {};
                        // 26.6 chain: SpellInfo+0x60 → SpellData → SpellData+0x80 = name
                        if (TryReadString(p + 0x80, buf, sizeof(buf)) && buf[0]) {
                            W("    SpellInfo+0x%02X → 0x%llX → +0x80 = \"%s\" <-- InfoSpellData candidate!\r\n",
                                off, (unsigned long long)p, buf);
                        }
                        // Also try SpellData+0x8 → Resource → Resource+0x80
                        uintptr_t res = Globals::Read<uintptr_t>(p + 0x8);
                        if (Globals::IsValidPtr(res)) {
                            buf[0] = 0;
                            if (TryReadString(res + 0x80, buf, sizeof(buf)) && buf[0]) {
                                W("    SpellInfo+0x%02X → +0x08 → 0x%llX → +0x80 = \"%s\"\r\n",
                                    off, (unsigned long long)res, buf);
                            }
                        }
                        // Try SpellData+0x28 (alternative name location)
                        buf[0] = 0;
                        if (TryReadString(p + 0x28, buf, sizeof(buf)) && buf[0]) {
                            W("    SpellInfo+0x%02X → 0x%llX → +0x28 = \"%s\"\r\n",
                                off, (unsigned long long)p, buf);
                        }
                    }
                }
            }
        }

        // ── 4. Brute-force SpellBook scan for "Ezreal" spell names ──
        W("\r\n--- SpellBook Brute-Force (scan obj for spell name pointers) ---\r\n");
        {
            char champName[64] = {};
            local.ReadCharacterName(champName, sizeof(champName));
            W("  Champion: %s — scanning for spell name strings containing this...\r\n", champName);

            const uintptr_t spellBook = obj + Offset::SpellBook::Offset;
            // Scan SpellBook region: try every 8-byte aligned qword as a pointer
            // then follow 2 levels deep looking for champion spell names
            int foundCount = 0;
            for (int sbOff = 0; sbOff <= 0x200 && foundCount < 20; sbOff += 8) {
                uintptr_t p1 = Globals::Read<uintptr_t>(spellBook + sbOff);
                if (!Globals::IsValidPtr(p1)) continue;

                // Level 1: p1 might be a SpellSlot — scan it for pointers to SpellData
                for (int slotOff = 0; slotOff <= 0x180 && foundCount < 20; slotOff += 8) {
                    uintptr_t p2 = Globals::Read<uintptr_t>(p1 + slotOff);
                    if (!Globals::IsValidPtr(p2)) continue;

                    // Level 2: p2 might be SpellData — scan for name strings
                    char buf[128] = {};
                    for (int nameOff = 0; nameOff <= 0x100; nameOff += 8) {
                        buf[0] = 0;
                        if (TryReadString(p2 + nameOff, buf, sizeof(buf)) && buf[0]) {
                            // Check if contains champion name prefix or looks like spell name
                            bool isSpellName = false;
                            if (champName[0] && strstr(buf, champName)) isSpellName = true;
                            if (strstr(buf, "Attack") || strstr(buf, "Spell")) isSpellName = true;
                            if (strlen(buf) >= 4 && strlen(buf) <= 40 && buf[0] >= 'A' && buf[0] <= 'Z') {
                                // Capitalized short string — could be spell name
                                bool allPrint = true;
                                for (int c = 0; buf[c]; ++c) {
                                    if (buf[c] < 0x20 || buf[c] > 0x7E) { allPrint = false; break; }
                                }
                                if (allPrint) isSpellName = true;
                            }
                            if (isSpellName) {
                                W("  SpellBook+0x%03X → +0x%03X → +0x%03X = \"%s\"\r\n",
                                    sbOff, slotOff, nameOff, buf);
                                foundCount++;
                            }
                        }
                    }
                }
            }
            if (foundCount == 0) {
                W("  No spell names found via brute-force.\r\n");
            }
        }

        // ── 5. ActiveSpellCast scan (try multiple offsets) ──
        W("\r\n--- ActiveSpellCast Offset Scan ---\r\n");
        {
            // Try reading ActiveSpellCast from different offsets around the known ones
            // SpellRuntime says 0x30F0, AIHeroClient says 0x3120
            const int candidates[] = { 0x30E0, 0x30E8, 0x30F0, 0x30F8, 0x3100, 0x3108, 0x3110, 0x3118, 0x3120, 0x3128 };
            for (int ci = 0; ci < 10; ++ci) {
                int off = candidates[ci];
                uintptr_t castPtr = Globals::Read<uintptr_t>(obj + off);
                if (!Globals::IsValidPtr(castPtr)) {
                    W("  obj+0x%04X: 0x%llX (invalid)\r\n", off, (unsigned long long)castPtr);
                    continue;
                }
                CoreSpellCastInfo::CastRef cast = { castPtr };
                int slot = cast.GetSlot();
                bool isAuto = cast.IsAutoAttack();
                char spName[64] = {};
                cast.ReadSpellName(spName, sizeof(spName));
                W("  obj+0x%04X: ptr=0x%llX slot=%d isAuto=%d name=\"%s\" %s\r\n",
                    off, (unsigned long long)castPtr, slot, isAuto ? 1 : 0,
                    spName[0] ? spName : "<empty>",
                    off == 0x30F0 ? "<-- SpellRuntime" :
                    off == 0x3120 ? "<-- AIHeroClient" : "");
            }
        }

        // ── 5. Verify key numeric offsets ──
        W("\r\n--- Numeric Field Verification ---\r\n");
        {
            W("  HP     (0x%X): %.1f\r\n", Offset::Health::HP, local.GetHealth());
            W("  MaxHP  (0x%X): %.1f\r\n", Offset::Health::MaxHP, local.GetMaxHealth());
            W("  MP     (0x%X): %.1f\r\n", Offset::Mana::MP, local.GetMana());
            W("  MaxMP  (0x%X): %.1f\r\n", Offset::Mana::MaxMP, local.GetMaxMana());
            W("  MS     (0x%X): %.1f\r\n", Offset::HeroStats::MoveSpeed, local.GetMoveSpeed());
            W("  Range  (0x%X): %.1f\r\n", Offset::HeroStats::AttackRange, local.GetAttackRange());
            W("  NetId  (0x%X): %d\r\n",   Offset::All::NetId, local.GetNetId());
            W("  Team   (0x%X): %d\r\n",   Offset::All::Team, local.GetTeam());
            W("  Pos    (0x%X): %.1f, %.1f, %.1f\r\n", Offset::All::Position,
                local.GetPosition().x, local.GetPosition().y, local.GetPosition().z);
        }

        // ── 6. Scan all heroes for Name/CharacterName ──
        W("\r\n--- All Heroes Name Scan ---\r\n");
        {
            uintptr_t heroAddrs[16] = {};
            int heroCount = CoreObjects::EnumerateHeroes(heroAddrs, 16);
            for (int i = 0; i < heroCount; ++i) {
                CoreObjects::ObjectRef hero = { heroAddrs[i] };
                if (!hero.IsValid()) continue;
                char name[64] = {}, champ[64] = {};
                hero.ReadName(name, sizeof(name));
                hero.ReadCharacterName(champ, sizeof(champ));
                W("  Hero[%d] netId=%d team=%d name=\"%s\" champ=\"%s\" hp=%.0f/%.0f\r\n",
                    i, hero.GetNetId(), hero.GetTeam(),
                    name[0] ? name : "<empty>", champ[0] ? champ : "<empty>",
                    hero.GetHealth(), hero.GetMaxHealth());
            }
        }

        // ── 7. SpellCastInfo::TargetIndex verification ──
        W("\r\n--- SpellCastInfo Layout (verified via IDA) ---\r\n");
        W("  SpellData       = 0x%X\r\n", Offset::SpellCastInfo::SpellData);
        W("  SrcIndex        = 0x%X\r\n", Offset::SpellCastInfo::SrcIndex);
        W("  TargetIndex     = 0x%X (FIXED: was 0x108, now 0x9C)\r\n", Offset::SpellCastInfo::TargetIndex);
        W("  StartPos        = 0x%X\r\n", Offset::SpellCastInfo::StartPos);
        W("  EndPos          = 0x%X\r\n", Offset::SpellCastInfo::EndPos);
        W("  CastPos         = 0x%X\r\n", Offset::SpellCastInfo::CastPos);
        W("  CastDelay       = 0x%X\r\n", Offset::SpellCastInfo::CastDelay);
        W("  IsSpell         = 0x%X\r\n", Offset::SpellCastInfo::IsSpell);
        W("  IsSpecialAttack = 0x%X\r\n", Offset::SpellCastInfo::IsSpecialAttack);
        W("  IsAuto          = 0x%X\r\n", Offset::SpellCastInfo::IsAuto);
        W("  Slot            = 0x%X\r\n", Offset::SpellCastInfo::Slot);

        W("\r\n=== Scan Complete ===\r\n");

        CloseHandle(hFile);

        // Build preview
        int previewLen = snprintf(m_preview, sizeof(m_preview),
            "Scan complete. Check C:\\Users\\Public\\ns_offset_scan.txt\n"
            "Name:  current=0x%X\n"
            "Champ: current=0x%X\n"
            "HP:    %.0f/%.0f\n"
            "NetId: %d",
            Offset::All::Name, Offset::All::CharacterName,
            local.GetHealth(), local.GetMaxHealth(), local.GetNetId());
        (void)previewLen;

        snprintf(m_lastStatus, sizeof(m_lastStatus), "OK — dumped to ns_offset_scan.txt");
        m_scanDone = true;
    }
};

} // namespace Plugins
