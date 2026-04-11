#pragma once

#include "../IPlugin.h"
#include "../../imgui/imgui.h"
#include "../../menu/NightSharpMenu.h"
#include "DebugLog.h"

namespace Plugins {

    class DebugWindowPlugin : public IPlugin {
    public:
        const char* GetName() const override { return "Debug Window"; }
        const char* GetInternalId() const override { return "debug_window"; }
        const char* GetAuthor() const override { return "NightSharp"; }
        PluginCategory GetCategory() const override { return PluginCategory::Core; }
        bool AutoLoadByDefault() const override { return true; }

        void OnRender() override {
            if (!NightSharpMenu::debugWindowEnabled) return;

            auto& s = DebugLogState::Get();

            ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowBgAlpha(0.65f);

            if (!ImGui::Begin("Debug Log", nullptr,
                ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoFocusOnAppearing)) {
                ImGui::End();
                return;
            }

            if (ImGui::Button("Clear")) {
                printClear();
            }

            ImGui::Separator();

            m_textBuf[0] = '\0';
            int pos = 0;
            for (int i = 0; i < s.Count; ++i) {
                const char* line;
                if (s.Count < DebugLogState::kMaxLines)
                    line = s.Lines[i];
                else
                    line = s.Lines[(s.WriteIndex + i) % DebugLogState::kMaxLines];
                int len = (int)strlen(line);
                if (pos + len + 1 < kTextBufSize) {
                    memcpy(m_textBuf + pos, line, len);
                    pos += len;
                    m_textBuf[pos++] = '\n';
                }
            }
            m_textBuf[pos] = '\0';

            ImVec2 avail = ImGui::GetContentRegionAvail();
            ImGui::InputTextMultiline("##debuglog", m_textBuf, kTextBufSize, avail,
                ImGuiInputTextFlags_ReadOnly);

            if (s.Generation != m_lastGen) {
                ImGui::SetScrollHereY(1.0f);
                m_lastGen = s.Generation;
            }

            ImGui::End();
        }

    private:
        static constexpr int kTextBufSize = DebugLogState::kMaxLines * DebugLogState::kMaxLineLen;
        char m_textBuf[kTextBufSize] = {};
        int m_lastGen = 0;
    };

}
