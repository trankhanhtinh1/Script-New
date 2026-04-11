#pragma once

#include "../imgui/imgui.h"
#include "MenuTheme.h"
#include "MenuUI.h"

namespace MenuRenderers {

using namespace MenuTheme;

inline void DrawBoolItem(ImDrawList* dl, ImVec2 pos, float panelX, float panelW,
                          SDK::MenuUI::MenuBool* b, bool drawSep = true) {
    ImVec2 mn = ImVec2(panelX, pos.y);
    ImVec2 mx = ImVec2(panelX + panelW, pos.y + ITEM_H);
    bool hovered = g_inputEnabled && ImGui::IsMouseHoveringRect(mn, mx, false);

    if (hovered)
        dl->AddRectFilled(mn, mx, COL_ITEM_HOVER, 3.0f);
    if (drawSep)
        dl->AddLine(ImVec2(panelX, mx.y), ImVec2(panelX + panelW, mx.y), COL_BORDER, 1.0f);

    dl->AddText(ImVec2(pos.x + 12, pos.y + 7), COL_TEXT, Translations::T(b->DisplayName.c_str()));

    const char* stateText = b->Enabled ? Translations::T("On") : Translations::T("Off");
    ImVec2 stateSize = ImGui::CalcTextSize(stateText);
    float padX = ImGui::GetStyle().FramePadding.x;
    float btnW = stateSize.x + padX * 2.0f;
    float btnX = panelX + panelW - btnW - 8.0f;
    ImU32 btnCol = b->Enabled ? IM_COL32(46, 140, 71, 250) : IM_COL32(166, 56, 61, 250);
    dl->AddRectFilled(ImVec2(btnX, pos.y + 4.0f), ImVec2(btnX + btnW, pos.y + ITEM_H - 4.0f), btnCol, 7.0f);
    dl->AddText(ImVec2(btnX + padX, pos.y + (ITEM_H - stateSize.y) * 0.5f),
                IM_COL32(245, 247, 255, 255), stateText);

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        b->Toggle();
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        b->ResetDefault();

    b->CheckChanged();
}

inline void DrawKeyBindItem(ImDrawList* dl, ImVec2 pos, float panelX, float panelW,
                              SDK::MenuUI::MenuKeyBind* kb, bool drawSep = true) {
    using KBType = SDK::MenuUI::KeyBindType;
    kb->PollListeningKey();

    ImVec2 mn = ImVec2(panelX, pos.y);
    ImVec2 mx = ImVec2(panelX + panelW, pos.y + ITEM_H);
    bool hovered = g_inputEnabled && ImGui::IsMouseHoveringRect(mn, mx, false);

    if (hovered)
        dl->AddRectFilled(mn, mx, COL_ITEM_HOVER, 3.0f);
    if (drawSep)
        dl->AddLine(ImVec2(panelX, mx.y), ImVec2(panelX + panelW, mx.y), COL_BORDER, 1.0f);

    dl->AddText(ImVec2(pos.x + 12, pos.y + 7), COL_TEXT, Translations::T(kb->DisplayName.c_str()));

    const char* pressText = Translations::T("Press");
    const char* toggleText = Translations::T("Toggle");
    const char* keyText = kb->IsListening() ? Translations::T("Press key...") : SDK::MenuUI::MenuKeyBind::GetKeyName(kb->Key);
    float padX = ImGui::GetStyle().FramePadding.x;
    float pad2 = padX * 2.0f;
    float pressW = ImGui::CalcTextSize(pressText).x + pad2;
    float toggleW = ImGui::CalcTextSize(toggleText).x + pad2;
    float keyTextW = ImGui::CalcTextSize(Translations::T("Press key...")).x;
    float keyNameW = ImGui::CalcTextSize(SDK::MenuUI::MenuKeyBind::GetKeyName(kb->Key)).x;
    float keyBtnW = ((keyTextW > keyNameW) ? keyTextW : keyNameW) + pad2;
    float gap = 4.0f;
    float totalW = pressW + gap + toggleW + gap + keyBtnW;
    float startX = panelX + panelW - totalW - 8.0f;
    float btnY = pos.y + 4.0f;
    float btnH = ITEM_H - 8.0f;

    ImU32 colActive = IM_COL32(77, 148, 87, 242);
    ImU32 colInactive = IM_COL32(36, 41, 61, 242);

    float cx = startX;
    ImVec2 pressMin = ImVec2(cx, btnY);
    ImVec2 pressMax = ImVec2(cx + pressW, btnY + btnH);
    dl->AddRectFilled(pressMin, pressMax, (kb->Type == KBType::Press) ? colActive : colInactive, 7.0f);
    ImVec2 pts = ImGui::CalcTextSize(pressText);
    dl->AddText(ImVec2(cx + (pressW - pts.x) * 0.5f, pos.y + (ITEM_H - pts.y) * 0.5f),
                IM_COL32(245, 247, 255, 255), pressText);

    cx += pressW + gap;
    ImVec2 toggleMin = ImVec2(cx, btnY);
    ImVec2 toggleMax = ImVec2(cx + toggleW, btnY + btnH);
    dl->AddRectFilled(toggleMin, toggleMax, (kb->Type == KBType::Toggle) ? colActive : colInactive, 7.0f);
    ImVec2 tts = ImGui::CalcTextSize(toggleText);
    dl->AddText(ImVec2(cx + (toggleW - tts.x) * 0.5f, pos.y + (ITEM_H - tts.y) * 0.5f),
                IM_COL32(245, 247, 255, 255), toggleText);

    cx += toggleW + gap;
    ImVec2 keyMin = ImVec2(cx, btnY);
    ImVec2 keyMax = ImVec2(cx + keyBtnW, btnY + btnH);
    dl->AddRectFilled(keyMin, keyMax, colInactive, 7.0f);
    ImVec2 kts = ImGui::CalcTextSize(keyText);
    dl->AddText(ImVec2(cx + (keyBtnW - kts.x) * 0.5f, pos.y + (ITEM_H - kts.y) * 0.5f),
                IM_COL32(245, 247, 255, 255), keyText);

    if (g_inputEnabled) {
        if (ImGui::IsMouseHoveringRect(pressMin, pressMax, false) && ImGui::IsMouseClicked(0) &&
            kb->Type != KBType::Press) {
            kb->Type = KBType::Press;
            kb->Active = false;
        }
        if (ImGui::IsMouseHoveringRect(toggleMin, toggleMax, false) && ImGui::IsMouseClicked(0) &&
            kb->Type != KBType::Toggle) {
            kb->Type = KBType::Toggle;
        }
        if (ImGui::IsMouseHoveringRect(keyMin, keyMax, false) && ImGui::IsMouseClicked(0)) {
            kb->StartListening();
        }
    }

    kb->CheckConfigChanged();
}

inline const void* s_expandedSlider = nullptr;
inline const void* s_draggingSlider = nullptr;

inline void DrawSliderTrack(ImDrawList* dl, float panelX, float panelW, float rowY, float t,
                              const void* slider) {
    float pad = 12.0f;
    float sliderX = panelX + pad;
    float sliderW = panelW - pad * 2.0f;
    float sliderH = 6.0f;
    float sliderY = rowY + (ITEM_H - sliderH) * 0.5f;

    dl->AddRectFilled(ImVec2(sliderX, sliderY), ImVec2(sliderX + sliderW, sliderY + sliderH),
                      IM_COL32(36, 41, 61, 242), 3.0f);
    dl->AddRectFilled(ImVec2(sliderX, sliderY), ImVec2(sliderX + sliderW * t, sliderY + sliderH),
                      IM_COL32(77, 148, 87, 242), 3.0f);

    float knobX = sliderX + sliderW * t;
    dl->AddCircleFilled(ImVec2(knobX, sliderY + sliderH * 0.5f), 6.0f, IM_COL32(220, 225, 240, 255));

    if (g_inputEnabled) {
        ImVec2 grabMin = ImVec2(sliderX - 6.0f, rowY);
        ImVec2 grabMax = ImVec2(sliderX + sliderW + 6.0f, rowY + ITEM_H);
        if (ImGui::IsMouseHoveringRect(grabMin, grabMax, false) && ImGui::IsMouseClicked(0))
            s_draggingSlider = slider;
    }
}

inline void DrawSliderItemInt(ImDrawList* dl, ImVec2 pos, float panelX, float panelW,
                               SDK::MenuUI::MenuSlider* s, bool drawSep = true) {
    bool expanded = (s_expandedSlider == s);
    float totalH = expanded ? ITEM_H * 2.0f : ITEM_H;

    ImVec2 mn = ImVec2(panelX, pos.y);
    ImVec2 mx = ImVec2(panelX + panelW, pos.y + totalH);
    bool hovered = g_inputEnabled && ImGui::IsMouseHoveringRect(mn, ImVec2(panelX + panelW, pos.y + ITEM_H), false);

    if (hovered)
        dl->AddRectFilled(mn, ImVec2(panelX + panelW, pos.y + ITEM_H), COL_ITEM_HOVER, 3.0f);
    if (drawSep)
        dl->AddLine(ImVec2(panelX, pos.y + totalH), ImVec2(panelX + panelW, pos.y + totalH), COL_BORDER, 1.0f);

    dl->AddText(ImVec2(pos.x + 12, pos.y + 7), COL_TEXT, Translations::T(s->DisplayName.c_str()));

    char valBuf[32];
    snprintf(valBuf, sizeof(valBuf), "%d", s->Value);
    ImVec2 valSize = ImGui::CalcTextSize(valBuf);
    dl->AddText(ImVec2(panelX + panelW - valSize.x - 8.0f, pos.y + (ITEM_H - valSize.y) * 0.5f), COL_TEXT, valBuf);

    if (g_inputEnabled && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        s_expandedSlider = expanded ? nullptr : s;
    if (g_inputEnabled && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        s->SetValue(s->MinValue);

    if (expanded) {
        float range = (float)(s->MaxValue - s->MinValue);
        float t = (range > 0.0f) ? (float)(s->Value - s->MinValue) / range : 0.0f;
        DrawSliderTrack(dl, panelX, panelW, pos.y + ITEM_H, t, s);

        if (s_draggingSlider == s) {
            if (ImGui::IsMouseDown(0)) {
                float pad = 12.0f;
                float sliderX = panelX + pad;
                float sliderW = panelW - pad * 2.0f;
                float mouseX = ImGui::GetIO().MousePos.x;
                float newT = std::clamp((mouseX - sliderX) / sliderW, 0.0f, 1.0f);
                s->Value = s->MinValue + (int)(newT * range + 0.5f);
                s->Value = std::clamp(s->Value, s->MinValue, s->MaxValue);
            } else {
                s_draggingSlider = nullptr;
            }
        }
    }
}

inline void DrawSliderItemFloat(ImDrawList* dl, ImVec2 pos, float panelX, float panelW,
                                  SDK::MenuUI::MenuSliderF* s, bool drawSep = true) {
    bool expanded = (s_expandedSlider == s);
    float totalH = expanded ? ITEM_H * 2.0f : ITEM_H;

    ImVec2 mn = ImVec2(panelX, pos.y);
    ImVec2 mx = ImVec2(panelX + panelW, pos.y + totalH);
    bool hovered = g_inputEnabled && ImGui::IsMouseHoveringRect(mn, ImVec2(panelX + panelW, pos.y + ITEM_H), false);

    if (hovered)
        dl->AddRectFilled(mn, ImVec2(panelX + panelW, pos.y + ITEM_H), COL_ITEM_HOVER, 3.0f);
    if (drawSep)
        dl->AddLine(ImVec2(panelX, pos.y + totalH), ImVec2(panelX + panelW, pos.y + totalH), COL_BORDER, 1.0f);

    dl->AddText(ImVec2(pos.x + 12, pos.y + 7), COL_TEXT, Translations::T(s->DisplayName.c_str()));

    char valBuf[32];
    snprintf(valBuf, sizeof(valBuf), "%.1f", (double)s->Value);
    ImVec2 valSize = ImGui::CalcTextSize(valBuf);
    dl->AddText(ImVec2(panelX + panelW - valSize.x - 8.0f, pos.y + (ITEM_H - valSize.y) * 0.5f), COL_TEXT, valBuf);

    if (g_inputEnabled && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        s_expandedSlider = expanded ? nullptr : s;
    if (g_inputEnabled && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        s->Value = s->MinValue;

    if (expanded) {
        float range = s->MaxValue - s->MinValue;
        float t = (range > 0.0f) ? (s->Value - s->MinValue) / range : 0.0f;
        DrawSliderTrack(dl, panelX, panelW, pos.y + ITEM_H, t, s);

        if (s_draggingSlider == s) {
            if (ImGui::IsMouseDown(0)) {
                float pad = 12.0f;
                float sliderX = panelX + pad;
                float sliderW = panelW - pad * 2.0f;
                float mouseX = ImGui::GetIO().MousePos.x;
                float newT = std::clamp((mouseX - sliderX) / sliderW, 0.0f, 1.0f);
                s->Value = s->MinValue + newT * range;
                s->Value = std::clamp(s->Value, s->MinValue, s->MaxValue);
            } else {
                s_draggingSlider = nullptr;
            }
        }
    }
}

inline void DrawListItem(ImDrawList* dl, ImVec2 pos, float panelX, float panelW,
                           SDK::MenuUI::MenuList* lst, bool drawSep = true) {
    ImVec2 mn = ImVec2(panelX, pos.y);
    ImVec2 mx = ImVec2(panelX + panelW, pos.y + ITEM_H);
    bool hovered = g_inputEnabled && ImGui::IsMouseHoveringRect(mn, mx, false);

    if (hovered)
        dl->AddRectFilled(mn, mx, COL_ITEM_HOVER, 3.0f);
    if (drawSep)
        dl->AddLine(ImVec2(panelX, mx.y), ImVec2(panelX + panelW, mx.y), COL_BORDER, 1.0f);

    dl->AddText(ImVec2(pos.x + 12, pos.y + 7), COL_TEXT, Translations::T(lst->DisplayName.c_str()));

    if (lst->Items.empty()) return;

    lst->Index = std::clamp(lst->Index, 0, (int)lst->Items.size() - 1);
    const char* current = lst->Items[lst->Index].c_str();

    float padX = ImGui::GetStyle().FramePadding.x;
    float pad2 = padX * 2.0f;
    float maxItemW = 0.0f;
    for (auto& item : lst->Items) {
        float w = ImGui::CalcTextSize(item.c_str()).x;
        if (w > maxItemW) maxItemW = w;
    }
    float itemBtnW = maxItemW + pad2;
    float arrowW = 18.0f;
    float gap = 4.0f;
    float totalW = arrowW + gap + itemBtnW + gap + arrowW;
    float startX = panelX + panelW - totalW - 8.0f;
    float btnY = pos.y + 4.0f;
    float btnH = ITEM_H - 8.0f;
    ImU32 colBtn = IM_COL32(36, 41, 61, 242);

    float cx = startX;
    ImVec2 leftMin = ImVec2(cx, btnY);
    ImVec2 leftMax = ImVec2(cx + arrowW, btnY + btnH);
    dl->AddRectFilled(leftMin, leftMax, colBtn, 7.0f);
    ImVec2 lts = ImGui::CalcTextSize("<");
    dl->AddText(ImVec2(cx + (arrowW - lts.x) * 0.5f, pos.y + (ITEM_H - lts.y) * 0.5f),
                IM_COL32(245, 247, 255, 255), "<");

    cx += arrowW + gap;
    ImVec2 itemMin = ImVec2(cx, btnY);
    ImVec2 itemMax = ImVec2(cx + itemBtnW, btnY + btnH);
    dl->AddRectFilled(itemMin, itemMax, colBtn, 7.0f);
    ImVec2 its = ImGui::CalcTextSize(current);
    dl->AddText(ImVec2(cx + (itemBtnW - its.x) * 0.5f, pos.y + (ITEM_H - its.y) * 0.5f),
                IM_COL32(245, 247, 255, 255), current);

    cx += itemBtnW + gap;
    ImVec2 rightMin = ImVec2(cx, btnY);
    ImVec2 rightMax = ImVec2(cx + arrowW, btnY + btnH);
    dl->AddRectFilled(rightMin, rightMax, colBtn, 7.0f);
    ImVec2 rts = ImGui::CalcTextSize(">");
    dl->AddText(ImVec2(cx + (arrowW - rts.x) * 0.5f, pos.y + (ITEM_H - rts.y) * 0.5f),
                IM_COL32(245, 247, 255, 255), ">");

    int prevIdx = lst->Index;
    if (g_inputEnabled) {
        if (ImGui::IsMouseHoveringRect(leftMin, leftMax, false) && ImGui::IsMouseClicked(0))
            lst->Index = (lst->Index - 1 + (int)lst->Items.size()) % (int)lst->Items.size();
        if (ImGui::IsMouseHoveringRect(rightMin, rightMax, false) && ImGui::IsMouseClicked(0))
            lst->Index = (lst->Index + 1) % (int)lst->Items.size();
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            lst->SetIndex(0);
    }
    if (lst->Index != prevIdx)
        lst->NotifyValueChanged();
}

inline void DrawSeparatorItem(ImDrawList* dl, ImVec2 pos, float panelX, float panelW,
                                SDK::MenuUI::MenuSeparator* sep) {
    if (!sep->DisplayName.empty()) {
        dl->AddText(ImVec2(pos.x + 12, pos.y + 7), IM_COL32(128, 179, 255, 255),
                    Translations::T(sep->DisplayName.c_str()));
    }
    dl->AddLine(ImVec2(panelX, pos.y + ITEM_H), ImVec2(panelX + panelW, pos.y + ITEM_H), COL_BORDER, 1.0f);
}

inline void DrawColorItem(ImDrawList* dl, ImVec2 pos, float panelX, float panelW,
                            SDK::MenuUI::MenuColor* c, bool drawSep = true) {
    ImVec2 mn = ImVec2(panelX, pos.y);
    ImVec2 mx = ImVec2(panelX + panelW, pos.y + ITEM_H);
    bool hovered = g_inputEnabled && ImGui::IsMouseHoveringRect(mn, mx, false);

    if (hovered)
        dl->AddRectFilled(mn, mx, COL_ITEM_HOVER, 3.0f);
    if (drawSep)
        dl->AddLine(ImVec2(panelX, mx.y), ImVec2(panelX + panelW, mx.y), COL_BORDER, 1.0f);

    dl->AddText(ImVec2(pos.x + 12, pos.y + 7), COL_TEXT, Translations::T(c->DisplayName.c_str()));

    float previewSize = ITEM_H - 10.0f;
    float previewX = panelX + panelW - previewSize - 8.0f;
    float previewY = pos.y + 5.0f;
    dl->AddRectFilled(ImVec2(previewX, previewY), ImVec2(previewX + previewSize, previewY + previewSize),
                      c->GetImU32(), 4.0f);
    dl->AddRect(ImVec2(previewX, previewY), ImVec2(previewX + previewSize, previewY + previewSize),
                COL_BORDER, 4.0f);

    static const void* s_activeColorPicker = nullptr;

    if (g_inputEnabled && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        s_activeColorPicker = (s_activeColorPicker == c) ? nullptr : c;
    }
    if (g_inputEnabled && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        c->ResetDefault();
        s_activeColorPicker = nullptr;
    }

    if (s_activeColorPicker == c) {
        ImVec2 pickerPos = ImVec2(panelX + panelW + PANEL_GAP, pos.y);
        ImGui::SetNextWindowPos(pickerPos, ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.04f, 0.05f, 0.07f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.35f, 0.39f, 0.58f, 0.7f));
        ImGui::Begin("##color_picker", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
        if (ImGui::ColorPicker4("##picker", c->Color,
            ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoInputs)) {
            c->NotifyValueChanged();
        }
        ImVec2 winMin = ImGui::GetWindowPos();
        ImVec2 winMax = ImVec2(winMin.x + ImGui::GetWindowSize().x, winMin.y + ImGui::GetWindowSize().y);
        ImGui::End();
        ImGui::PopStyleColor(2);

        bool inRow = ImGui::IsMouseHoveringRect(mn, mx, false);
        bool inPicker = ImGui::IsMouseHoveringRect(winMin, winMax, false);
        if (ImGui::IsMouseClicked(0) && !inRow && !inPicker) {
            s_activeColorPicker = nullptr;
        }
    }
}

inline void DrawGenericItem(ImDrawList* dl, ImVec2 pos, float panelX, float panelW,
                              SDK::MenuItem* item, bool drawSep = true) {
    ImVec2 mn = ImVec2(panelX, pos.y);
    ImVec2 mx = ImVec2(panelX + panelW, pos.y + ITEM_H);
    bool hovered = g_inputEnabled && ImGui::IsMouseHoveringRect(mn, mx, false);

    if (hovered)
        dl->AddRectFilled(mn, mx, COL_ITEM_HOVER, 3.0f);
    if (drawSep)
        dl->AddLine(ImVec2(panelX, mx.y), ImVec2(panelX + panelW, mx.y), COL_BORDER, 1.0f);

    dl->AddText(ImVec2(pos.x + 12, pos.y + 7), COL_TEXT, Translations::T(item->DisplayName.c_str()));
}

inline float EstimateItemHeight(SDK::MenuItem* item) {
    if (!item) return 0.0f;
    if (auto* s = dynamic_cast<SDK::MenuUI::MenuSlider*>(item))
        return (s_expandedSlider == s) ? ITEM_H * 2.0f : ITEM_H;
    if (auto* sf = dynamic_cast<SDK::MenuUI::MenuSliderF*>(item))
        return (s_expandedSlider == sf) ? ITEM_H * 2.0f : ITEM_H;
    return ITEM_H;
}

inline float DrawItem(ImDrawList* dl, ImVec2 pos, float panelX, float panelW,
                        SDK::MenuItem* item, bool drawSep = true) {
    if (!item) return 0.0f;

    if (auto* b = dynamic_cast<SDK::MenuUI::MenuBool*>(item)) {
        DrawBoolItem(dl, pos, panelX, panelW, b, drawSep);
        return ITEM_H;
    }
    if (auto* kb = dynamic_cast<SDK::MenuUI::MenuKeyBind*>(item)) {
        DrawKeyBindItem(dl, pos, panelX, panelW, kb, drawSep);
        return ITEM_H;
    }
    if (auto* s = dynamic_cast<SDK::MenuUI::MenuSlider*>(item)) {
        DrawSliderItemInt(dl, pos, panelX, panelW, s, drawSep);
        return (s_expandedSlider == s) ? ITEM_H * 2.0f : ITEM_H;
    }
    if (auto* sf = dynamic_cast<SDK::MenuUI::MenuSliderF*>(item)) {
        DrawSliderItemFloat(dl, pos, panelX, panelW, sf, drawSep);
        return (s_expandedSlider == sf) ? ITEM_H * 2.0f : ITEM_H;
    }
    if (auto* lst = dynamic_cast<SDK::MenuUI::MenuList*>(item)) {
        DrawListItem(dl, pos, panelX, panelW, lst, drawSep);
        return ITEM_H;
    }
    if (auto* sep = dynamic_cast<SDK::MenuUI::MenuSeparator*>(item)) {
        DrawSeparatorItem(dl, pos, panelX, panelW, sep);
        return ITEM_H;
    }
    if (auto* col = dynamic_cast<SDK::MenuUI::MenuColor*>(item)) {
        DrawColorItem(dl, pos, panelX, panelW, col, drawSep);
        return ITEM_H;
    }

    DrawGenericItem(dl, pos, panelX, panelW, item, drawSep);
    return ITEM_H;
}

} // namespace MenuRenderers
