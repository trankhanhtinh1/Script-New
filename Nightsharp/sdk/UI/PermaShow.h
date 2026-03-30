#pragma once

#include "UI.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace SDK::UI::PermaShow {

    struct Entry {
        MenuItem* Item = nullptr;
        std::string DisplayName;
        ImU32 Color = IM_COL32(255, 255, 255, 255);
    };

    inline std::vector<Entry>* g_entries = nullptr;
    inline Menu* g_settingsMenu = nullptr;

    inline std::vector<Entry>& Entries() {
        if (!g_entries) {
            g_entries = new std::vector<Entry>();
        }
        return *g_entries;
    }

    inline const char* GetKeyName(int vk) {
        static char nameBuf[64];
        switch (vk) {
        case 0: return "None";
        case VK_SPACE: return "Space";
        case VK_LBUTTON: return "Mouse1";
        case VK_RBUTTON: return "Mouse2";
        case VK_MBUTTON: return "MMB";
        case VK_LSHIFT: return "LShift";
        case VK_RSHIFT: return "RShift";
        case VK_LCONTROL: return "LCtrl";
        case VK_RCONTROL: return "RCtrl";
        case VK_LMENU: return "LAlt";
        case VK_RMENU: return "RAlt";
        case VK_CAPITAL: return "CapsLock";
        case VK_TAB: return "Tab";
        case VK_RETURN: return "Enter";
        case VK_ESCAPE: return "Esc";
        case VK_BACK: return "Backspace";
        case VK_DELETE: return "Delete";
        case VK_INSERT: return "Insert";
        case VK_HOME: return "Home";
        case VK_END: return "End";
        case VK_PRIOR: return "PageUp";
        case VK_NEXT: return "PageDown";
        case VK_LEFT: return "Left";
        case VK_RIGHT: return "Right";
        case VK_UP: return "Up";
        case VK_DOWN: return "Down";
        case VK_OEM_3: return "`";
        default:
            if (vk >= '0' && vk <= '9') {
                nameBuf[0] = static_cast<char>(vk);
                nameBuf[1] = 0;
                return nameBuf;
            }
            if (vk >= 'A' && vk <= 'Z') {
                nameBuf[0] = static_cast<char>(vk);
                nameBuf[1] = 0;
                return nameBuf;
            }
            if (vk >= VK_F1 && vk <= VK_F12) {
                std::snprintf(nameBuf, sizeof(nameBuf), "F%d", (vk - VK_F1) + 1);
                return nameBuf;
            }

            UINT scanCode = MapVirtualKeyA(static_cast<UINT>(vk), MAPVK_VK_TO_VSC);
            if (scanCode != 0) {
                LONG lParam = static_cast<LONG>(scanCode << 16);
                int len = GetKeyNameTextA(lParam, nameBuf, static_cast<int>(sizeof(nameBuf)));
                if (len > 0) {
                    return nameBuf;
                }
            }
            return "?";
        }
    }

    inline bool IsEnabled() {
        return !g_settingsMenu || g_settingsMenu->GetBoolValue("enablepermashow", true);
    }

    inline float X() {
        return g_settingsMenu ? static_cast<float>(g_settingsMenu->GetSliderValue("X", 435)) : 435.0f;
    }

    inline float Y() {
        return g_settingsMenu ? static_cast<float>(g_settingsMenu->GetSliderValue("Y", 20)) : 20.0f;
    }

    inline float Width() {
        return g_settingsMenu ? static_cast<float>(g_settingsMenu->GetSliderValue("bwidth", 250)) : 250.0f;
    }

    inline float IndicatorWidth() {
        return g_settingsMenu ? static_cast<float>(g_settingsMenu->GetSliderValue("swidth", 45)) : 45.0f;
    }

    inline void Initialize(Menu* parent = nullptr) {
        if (g_settingsMenu) {
            return;
        }

        if (parent) {
            g_settingsMenu = parent->AddSubMenu("permashow", "Permashow");
        } else {
            g_settingsMenu = Menu::Create("permashow", "Permashow");
            if (g_settingsMenu) {
                g_settingsMenu->Attach();
            }
        }

        if (!g_settingsMenu) {
            return;
        }

        g_settingsMenu->Add<MenuBool>("enablepermashow", "Enable PermaShow", true);
        g_settingsMenu->Add<MenuSlider>("X", "X", 435, 0, 3840);
        g_settingsMenu->Add<MenuSlider>("Y", "Y", 20, 0, 2160);
        g_settingsMenu->Add<MenuSlider>("bwidth", "Width", 250, 100, 400);
        g_settingsMenu->Add<MenuSlider>("swidth", "Indicator Width", 45, 30, 90);
    }

    inline void Reset() {
        if (g_entries) {
            g_entries->clear();
        }
    }

    inline void Clear() {
        Entries().clear();
    }

    inline void Remove(MenuItem* item) {
        if (!item || !g_entries) {
            return;
        }
        auto& entries = Entries();
        entries.erase(
            std::remove_if(entries.begin(), entries.end(),
                [&](const Entry& entry) { return entry.Item == item; }),
            entries.end());
    }

    inline void Remove(const std::string& internalName) {
        if (internalName.empty() || !g_entries) {
            return;
        }
        auto& entries = Entries();
        entries.erase(
            std::remove_if(entries.begin(), entries.end(),
                [&](const Entry& entry) {
                    return entry.Item && entry.Item->InternalName == internalName;
                }),
            entries.end());
    }

    inline bool Contains(MenuItem* item) {
        if (!item || !g_entries) {
            return false;
        }
        const auto& entries = Entries();
        return std::any_of(entries.begin(), entries.end(),
            [&](const Entry& entry) { return entry.Item == item; });
    }

    inline void Permashow(MenuItem* item,
                          bool enabled = true,
                          const char* customDisplayName = nullptr,
                          ImU32 color = IM_COL32(255, 255, 255, 255)) {
        if (!item) {
            return;
        }

        if (!enabled) {
            Remove(item);
            return;
        }

        auto& entries = Entries();
        auto it = std::find_if(entries.begin(), entries.end(),
            [&](const Entry& entry) { return entry.Item == item; });

        const std::string displayName =
            (customDisplayName && customDisplayName[0]) ? customDisplayName : item->DisplayName;

        if (it != entries.end()) {
            it->DisplayName = displayName;
            it->Color = color;
            return;
        }

        entries.push_back({ item, displayName, color });
    }

    template<typename T>
    inline void Permashow(T* item,
                          bool enabled = true,
                          const char* customDisplayName = nullptr,
                          ImU32 color = IM_COL32(255, 255, 255, 255)) {
        Permashow(static_cast<MenuItem*>(item), enabled, customDisplayName, color);
    }

    inline void Permashow(MenuItem& item,
                          bool enabled = true,
                          const char* customDisplayName = nullptr,
                          ImU32 color = IM_COL32(255, 255, 255, 255)) {
        Permashow(&item, enabled, customDisplayName, color);
    }

    inline bool TryGetBooleanState(MenuItem* item, bool& value) {
        if (const auto* boolean = dynamic_cast<MenuBool*>(item)) {
            value = boolean->Enabled;
            return true;
        }
        if (const auto* keyBind = dynamic_cast<MenuKeyBind*>(item)) {
            value = keyBind->Active;
            return true;
        }
        return false;
    }

    inline std::string GetValueText(MenuItem* item) {
        if (!item) {
            return "-";
        }

        if (const auto* boolean = dynamic_cast<MenuBool*>(item)) {
            return boolean->Enabled ? "True" : "False";
        }

        if (const auto* slider = dynamic_cast<MenuSlider*>(item)) {
            return std::to_string(slider->Value);
        }

        if (const auto* sliderF = dynamic_cast<MenuSliderF*>(item)) {
            char buffer[32] = {};
            std::snprintf(buffer, sizeof(buffer), "%.2f", sliderF->Value);
            return buffer;
        }

        if (const auto* keyBind = dynamic_cast<MenuKeyBind*>(item)) {
            std::string result = GetKeyName(keyBind->Key);
            result += keyBind->Active ? " : True" : " : False";
            return result;
        }

        if (const auto* list = dynamic_cast<MenuList*>(item)) {
            return list->GetSelectedString();
        }

        if (const auto* color = dynamic_cast<MenuColor*>(item)) {
            char buffer[48] = {};
            std::snprintf(buffer, sizeof(buffer), "RGBA(%d,%d,%d,%d)",
                static_cast<int>(color->Color[0] * 255.0f),
                static_cast<int>(color->Color[1] * 255.0f),
                static_cast<int>(color->Color[2] * 255.0f),
                static_cast<int>(color->Color[3] * 255.0f));
            return buffer;
        }

        return "Unsupported";
    }

    inline void Render() {
        if (!IsEnabled() || Entries().empty()) {
            return;
        }

        ImDrawList* drawList = ImGui::GetBackgroundDrawList();
        if (!drawList) {
            return;
        }

        const float x = X();
        const float y = Y();
        const float width = Width();
        const float smallBoxWidth = IndicatorWidth();
        const float padding = 8.0f;
        const float lineHeight = ImGui::GetFontSize() * 1.35f;
        const float titleHeight = lineHeight + 4.0f;
        const float totalHeight = padding * 2.0f + titleHeight + static_cast<float>(Entries().size()) * lineHeight;

        drawList->AddRectFilled(
            ImVec2(x, y),
            ImVec2(x + width, y + totalHeight),
            IM_COL32(20, 20, 30, 210),
            4.0f);

        drawList->AddRect(
            ImVec2(x, y),
            ImVec2(x + width, y + totalHeight),
            IM_COL32(80, 80, 120, 180),
            4.0f);

        drawList->AddText(
            ImVec2(x + padding, y + padding - 1.0f),
            IM_COL32(60, 140, 230, 255),
            "Permashow");

        float currentY = y + padding + titleHeight;
        for (const auto& entry : Entries()) {
            if (!entry.Item) {
                currentY += lineHeight;
                continue;
            }

            bool hasBooleanState = false;
            bool isOn = false;
            hasBooleanState = TryGetBooleanState(entry.Item, isOn);

            const std::string valueText = GetValueText(entry.Item);
            const ImVec2 valueSize = ImGui::CalcTextSize(valueText.c_str());

            drawList->AddText(
                ImVec2(x + padding, currentY),
                entry.Color,
                entry.DisplayName.c_str());

            const float valueX = x + width - padding - valueSize.x;
            drawList->AddText(
                ImVec2(valueX, currentY),
                entry.Color,
                valueText.c_str());

            if (hasBooleanState) {
                const float boxRight = valueX - 8.0f;
                const float boxLeft = boxRight - smallBoxWidth;
                drawList->AddRectFilled(
                    ImVec2(boxLeft, currentY + 1.0f),
                    ImVec2(boxRight, currentY + lineHeight - 3.0f),
                    isOn ? IM_COL32(0, 110, 50, 180) : IM_COL32(120, 35, 35, 180),
                    2.0f);
            }

            currentY += lineHeight;
        }
    }

} // namespace SDK::UI::PermaShow
