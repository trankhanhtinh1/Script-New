#pragma once

#include "DebugLog.h"
#include "imgui/imgui.h"

#include <Windows.h>
#include <cstdio>

namespace NightSharpDebug::InternalConsole {

inline bool Visible = false;
inline DWORD LastToggleTick = 0;
inline bool AutoScroll = true;

inline void Toggle() {
    Visible = !Visible;
    NightSharpDebug::Logf(
        "[InternalConsole] %s",
        Visible ? "shown" : "hidden");
}

inline void ToggleHotkey() {
    if ((GetAsyncKeyState(VK_F5) & 1) == 0) {
        return;
    }

    const DWORD now = GetTickCount();
    if (LastToggleTick != 0 && now - LastToggleTick < 180) {
        return;
    }
    LastToggleTick = now;
    Toggle();
}

inline void Render() {
    if (!Visible || !ImGui::GetCurrentContext()) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 display = io.DisplaySize;
    const float width = display.x > 0.0f ? display.x * 0.58f : 820.0f;
    const float height = display.y > 0.0f ? display.y * 0.38f : 360.0f;
    const float x = display.x > 0.0f ? display.x - width - 18.0f : 18.0f;
    const float y = 72.0f;

    ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings;

    bool open = Visible;
    if (!ImGui::Begin("NightSharp Internal Debug Console (F5)", &open, flags)) {
        Visible = open;
        ImGui::End();
        return;
    }
    Visible = open;

    char phase[128] = {};
    NightSharpDebug::GetPhase(phase, sizeof(phase));

    NightSharpDebug::LockInternalConsole();
    const int lineCount = NightSharpDebug::InternalConsoleLineCountUnsafe();
    const unsigned dropped = NightSharpDebug::InternalConsoleDroppedUnsafe();
    NightSharpDebug::UnlockInternalConsole();

    ImGui::Text("phase: %s", phase);
    ImGui::SameLine();
    ImGui::Text("lines: %d", lineCount);
    if (dropped != 0) {
        ImGui::SameLine();
        ImGui::Text("dropped: %u", dropped);
    }

    if (ImGui::Button("Clear")) {
        NightSharpDebug::ClearInternalConsole();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &AutoScroll);
    ImGui::Separator();

    ImGui::BeginChild(
        "NightSharpInternalDebugConsoleScroll",
        ImVec2(0.0f, 0.0f),
        true,
        ImGuiWindowFlags_HorizontalScrollbar);

    NightSharpDebug::LockInternalConsole();
    const int count = NightSharpDebug::InternalConsoleLineCountUnsafe();
    for (int i = 0; i < count; ++i) {
        ImGui::TextUnformatted(NightSharpDebug::InternalConsoleLineUnsafe(i));
    }
    NightSharpDebug::UnlockInternalConsole();

    if (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
    ImGui::End();
}

} // namespace NightSharpDebug::InternalConsole
