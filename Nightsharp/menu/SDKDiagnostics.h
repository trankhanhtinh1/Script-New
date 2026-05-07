#pragma once
// ============================================================================
// SDKDiagnostics — "SDK Diagnostics" tab inside NightSharp menu
// ============================================================================
// Renders a compact dashboard showing, at a glance, whether every event hook
// is actually working:
//
//   Panel 1 — Core hook (OnProcessSpell Shadow-VMT):
//       install state, dispatch slot address, trampoline address, fire count.
//   Panel 2 — Per-event table:
//       name, hook method, "Ready / Disabled" flag, fire counter (live).
//
// The panel does NOT assume `CoreEventHook::PollAllEvents()` has been wired up
// yet; it just reflects whatever counters the rest of the DLL feeds in.
// ============================================================================

#include "../imgui/imgui.h"
#include "../core/CoreEventHook.h"

#include <cstdint>
#include <cstdio>

namespace SDKDiagnostics {

    namespace detail {
        inline void Badge(const char* text, ImU32 bg, ImU32 fg = IM_COL32(12, 12, 18, 255)) {
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            ImVec2 sz = ImGui::CalcTextSize(text);
            ImVec2 p1 = ImVec2(p0.x + sz.x + 10, p0.y + sz.y + 4);
            auto* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(p0, p1, bg, 3.0f);
            dl->AddText(ImVec2(p0.x + 5, p0.y + 2), fg, text);
            ImGui::Dummy(ImVec2(sz.x + 10, sz.y + 4));
        }

        inline void Hex64(char* out, size_t cap, uint64_t v) {
            std::snprintf(out, cap, "0x%016llX", static_cast<unsigned long long>(v));
        }
    }

    inline void Render() {
        using namespace CoreEventHook;

        ImGui::Dummy(ImVec2(0, 4));

        // ---------------- Panel 1 — Shadow-VMT core hook status ----------------
        // The Shadow-VMT hook is now OPTIONAL. `OnProcessSpell` is driven by
        // the spell-cast poller in `PollSpellCast`, which works on every
        // build. The VMT hook remains installed as a secondary detector
        // for legacy builds; on 26.6+ it is expected to show "NOT FIRING".
        ImGui::TextColored(ImVec4(0.47f, 0.92f, 0.47f, 1.0f), "Shadow-VMT (legacy, optional)");
        ImGui::Separator();

        const bool installed  = diagnostics::HookInstalled();
        const bool installing = diagnostics::HookInstalling();
        ImGui::Text("State: ");
        ImGui::SameLine();
        if (installed) {
            detail::Badge(" HOOKED ", IM_COL32(60, 200, 90, 255));
        } else if (installing) {
            detail::Badge(" INSTALLING... ", IM_COL32(200, 160, 60, 255));
        } else {
            detail::Badge(" NOT HOOKED ", IM_COL32(140, 140, 140, 255), IM_COL32(255, 255, 255, 255));
        }

        char buf[64];
        detail::Hex64(buf, sizeof(buf), diagnostics::DispatchSlotAddr());
        ImGui::Text("Dispatch slot : %s   (%d writable copies)",
                    buf, diagnostics::DispatchSlotCount());
        detail::Hex64(buf, sizeof(buf), diagnostics::TrampolineAddr());
        ImGui::Text("Trampoline    : %s", buf);
        detail::Hex64(buf, sizeof(buf), diagnostics::OriginalFnAddr());
        ImGui::Text("Original fn   : %s", buf);

        const auto vmtCount = diagnostics::VmtEventCounter();
        ImGui::Text("VMT counter   : %llu", static_cast<unsigned long long>(vmtCount));
        ImGui::SameLine();
        if (installed) {
            if (vmtCount > 0) {
                detail::Badge(" FIRING ", IM_COL32(60, 200, 90, 255));
            } else {
                // Expected on 26.6+ where the vtable slot no longer routes
                // OnProcessSpell. Using grey instead of red so it isn't
                // mistaken for an error — polling handles the event.
                detail::Badge(" IDLE (poll fallback) ", IM_COL32(140, 140, 140, 255), IM_COL32(255, 255, 255, 255));
            }
        }
        ImGui::Text("Total events  : %llu",
                    static_cast<unsigned long long>(diagnostics::TotalFires()));

        ImGui::Dummy(ImVec2(0, 8));

        // ---------------- Manual install / uninstall buttons -------------------
        if (!installed) {
            if (ImGui::Button("Install Hook")) {
                (void)InstallAllHooks();
            }
        } else {
            if (ImGui::Button("Uninstall All")) {
                UninstallAll();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Force Poll")) {
            PollAllEvents();
        }

        ImGui::Dummy(ImVec2(0, 8));

        // ---------------- Panel 2 — Per-event table ----------------------------
        ImGui::TextColored(ImVec4(0.47f, 0.92f, 0.47f, 1.0f), "Event Hooks");
        ImGui::Separator();

        if (ImGui::BeginTable("##event_table", 4,
                              ImGuiTableFlags_SizingFixedFit |
                              ImGuiTableFlags_BordersInnerV  |
                              ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Event",   ImGuiTableColumnFlags_WidthFixed, 170);
            ImGui::TableSetupColumn("Method",  ImGuiTableColumnFlags_WidthFixed, 115);
            ImGui::TableSetupColumn("Status",  ImGuiTableColumnFlags_WidthFixed, 110);
            ImGui::TableSetupColumn("Fires",   ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (int i = 0; i < diagnostics::kAllEventCount; ++i) {
                const int id      = diagnostics::kAllEventIds[i];
                const auto method = diagnostics::MethodOf(id);
                const bool ready  = diagnostics::IsEventReady(id);
                const auto fires  = diagnostics::FireCountOf(id);
                const bool isVmt  = (method == diagnostics::Method::VmtHook);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(diagnostics::NameOf(id));

                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(diagnostics::MethodLabel(method));

                ImGui::TableSetColumnIndex(2);
                if (isVmt) {
                    if (!installed) {
                        detail::Badge(" FAIL ", IM_COL32(210, 70, 70, 255), IM_COL32(255, 255, 255, 255));
                    } else if (fires > 0) {
                        // Any fires count — regardless of whether the VMT
                        // trampoline is the path that delivered them — means
                        // the event is functionally live. Poll fallback
                        // absolutely counts as "working".
                        detail::Badge(" ACTIVE ", IM_COL32(60, 180, 200, 255));
                    } else if (vmtCount == 0) {
                        // Hook installed, but trampoline never fired AND no
                        // fallback caught the event → dispatch slot bad and
                        // poll didn't detect either. Truly dead.
                        detail::Badge(" STALE ", IM_COL32(210, 120, 50, 255), IM_COL32(255, 255, 255, 255));
                    } else {
                        // VMT trampoline is alive (other events got dispatched)
                        // but this specific event hasn't been observed yet.
                        detail::Badge(" IDLE ", IM_COL32(180, 160, 60, 255));
                    }
                } else if (!ready) {
                    detail::Badge(" DISABLED ", IM_COL32(140, 140, 140, 255), IM_COL32(255, 255, 255, 255));
                } else if (fires == 0) {
                    detail::Badge(" IDLE ", IM_COL32(180, 160, 60, 255));
                } else {
                    detail::Badge(" ACTIVE ", IM_COL32(60, 180, 200, 255));
                }

                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%llu", static_cast<unsigned long long>(fires));
            }
            ImGui::EndTable();
        }

        ImGui::Dummy(ImVec2(0, 6));
        ImGui::TextColored(ImVec4(0.72f, 0.72f, 0.80f, 1.0f),
            "All events are driven by per-frame polling on this build.");
        ImGui::TextColored(ImVec4(0.72f, 0.72f, 0.80f, 1.0f),
            "IDLE = poller armed, waiting for an in-game trigger.");
        ImGui::TextColored(ImVec4(0.72f, 0.72f, 0.80f, 1.0f),
            "ACTIVE = event fired at least once; counter on the right.");
        ImGui::TextColored(ImVec4(0.72f, 0.72f, 0.80f, 1.0f),
            "DISABLED = offsets still 0x0 in offset.h — re-verify.");

        // ---------------- Panel 3 — Inline Detour install report --------------
        // Surfaces WHY each of the 8 inline-detour hooks failed, so we can
        // tell cloak-missed vs decoder-missed vs bad-args from the overlay
        // without needing debugger output.
        ImGui::Dummy(ImVec2(0, 10));
        ImGui::TextColored(ImVec4(0.47f, 0.92f, 0.47f, 1.0f),
            "Inline Detours (DBVM EPT-cloaked)");
        ImGui::Separator();

        // --- Direct-syscall stealth probe (tells us whether Packman blocks
        //     code-page flips, and if so, whether PAGE_READONLY still works). ---
        {
            using namespace CoreEventHook::stealth;
            ImGui::Text("Syscall SSN   : 0x%X  (source: %s)",
                (unsigned)g_ssnCached,
                g_ssnSource == 1 ? "in-memory ntdll" :
                g_ssnSource == 2 ? "ntdll from disk" :
                                    "NOT RESOLVED");
            ImGui::Text("Last NTSTATUS : 0x%08X",
                static_cast<unsigned>(g_lastStatus));

            const int st = g_selfTest;
            const char* stLabel =
                st == 0 ? "not run"  :
                st == 1 ? "RWX flip OK  (Packman NOT blocking protect flips)" :
                st == 2 ? "RWX blocked, PAGE_READONLY OK  (CoW inline detour path)" :
                          "BOTH blocked  (kernel-side filter — DBVM/driver needed)";
            ImGui::Text("Self-test     : %d - %s", st, stLabel);
            if (st >= 2) {
                ImGui::Text("  RWX status  : 0x%08X", (unsigned)g_selfTestStatusRWX);
            }
            if (st == 3) {
                ImGui::Text("  RO  status  : 0x%08X", (unsigned)g_selfTestStatusRO);
            }

            // NtWriteVirtualMemory probe — does the write syscall bypass
            // STATUS_SECTION_PROTECTION? If so, inline-detour is feasible.
            const int wst = g_writeSelfTest;
            const char* wstLabel =
                wst == 0 ? "not run" :
                wst == 1 ? "WriteVM OK   (inline detour possible!)" :
                           "WriteVM FAILED (need DBVM / HWBP fallback)";
            ImGui::Text("Write probe   : %d - %s  (SSN=0x%X, status=0x%08X)",
                        wst, wstLabel,
                        (unsigned)g_writeSsn,
                        (unsigned)g_writeSelfTestStatus);

            // Copy-on-Write probe — flipping image pages to PAGE_EXECUTE_WRITECOPY
            // is often allowed even when +W/+RWX is refused (OS lets CoW semantics
            // through since they don't actually grant persistent write on the
            // shared mapping — the write upgrades to a private page).
            const int ct = g_cowSelfTest;
            const char* ctLabel =
                ct == 0 ? "not run" :
                ct == 1 ? "EXECUTE_WRITECOPY OK  (CoW detour feasible!)" :
                ct == 2 ? "only PAGE_WRITECOPY OK (need RX flip-back)" :
                          "CoW blocked (kernel enforces no COW on this page)";
            ImGui::Text("CoW probe     : %d - %s  (EWC=0x%08X, WC=0x%08X)",
                        ct, ctLabel,
                        (unsigned)g_cowSelfTestStatusEWC,
                        (unsigned)g_cowSelfTestStatusWC);
        }
        ImGui::Dummy(ImVec2(0, 4));

        int inlineOk = 0;
        for (int s = 0; s < 8; ++s) {
            if (CoreEventHook::g_InlineStatus[4 + s] == CoreEventHook::detail::kInst_OK) {
                ++inlineOk;
            }
        }
        const bool issueOrderOk = CoreEventHook::detail::g_detOnIssueOrder.installed;
        if (issueOrderOk) ++inlineOk;
        ImGui::Text("Installed: %d / 9  (8 typed + 1 raw-asm)", inlineOk);

        if (ImGui::BeginTable("##inline_table", 2,
                              ImGuiTableFlags_SizingFixedFit |
                              ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Event",  ImGuiTableColumnFlags_WidthFixed, 220);
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (int s = 0; s < 8; ++s) {
                const uint8_t code = CoreEventHook::g_InlineStatus[4 + s];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(CoreEventHook::InlineStatusName(s));
                ImGui::TableSetColumnIndex(1);
                if (code == CoreEventHook::detail::kInst_OK) {
                    detail::Badge(" OK ", IM_COL32(60, 200, 90, 255));
                } else if (code == CoreEventHook::detail::kInst_NotAttempted) {
                    detail::Badge(" SKIPPED ", IM_COL32(140, 140, 140, 255),
                                  IM_COL32(255, 255, 255, 255));
                } else {
                    detail::Badge(CoreEventHook::InlineStatusLabel(code),
                                  IM_COL32(210, 70, 70, 255),
                                  IM_COL32(255, 255, 255, 255));
                }
            }

            // Extra row: OnIssueOrder (raw-asm trampoline, not in g_InlineStatus).
            {
                const auto& d = CoreEventHook::detail::g_detOnIssueOrder;
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("OnIssueOrder (raw-asm)");
                ImGui::TableSetColumnIndex(1);
                if (d.installed) {
                    detail::Badge(" OK ", IM_COL32(60, 200, 90, 255));
                    ImGui::SameLine();
                    ImGui::Text("hits=%lld", (long long)d.hitCounter);
                } else if (d.lastStatus == CoreEventHook::detail::kInst_NotAttempted) {
                    detail::Badge(" SKIPPED ", IM_COL32(140, 140, 140, 255),
                                  IM_COL32(255, 255, 255, 255));
                } else {
                    detail::Badge(CoreEventHook::InlineStatusLabel(d.lastStatus),
                                  IM_COL32(210, 70, 70, 255),
                                  IM_COL32(255, 255, 255, 255));
                }
            }
            ImGui::EndTable();
        }

        // Panel 4 (DEP Hook / Vault7) — REMOVED.
        // The DEP-hook mechanism used to provide a "no-code-write" fallback
        // for targets where inline detouring hit STATUS_SECTION_PROTECTION.
        // In practice, arming it flipped live LoL.exe code-page protection
        // bits which Packman detected as tampering and crashed the game.
        // We now run ONLY Shadow-VMT (virtual method table swap) and
        // Inline+EPT (CoW-backed inline detour) — both of which avoid any
        // page-protection changes that Packman can observe.
    }

} // namespace SDKDiagnostics
