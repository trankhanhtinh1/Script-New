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
    const char* modeText = (kb->Type == KBType::Toggle) ? toggleText : pressText;
    const char* keyText = kb->IsListening() ? Translations::T("Press key...") : SDK::MenuUI::MenuKeyBind::GetKeyName(kb->Key);
    float padX = ImGui::GetStyle().FramePadding.x;
    float pad2 = padX * 2.0f;
    float modeTextW = ImGui::CalcTextSize(pressText).x;
    float toggleTextW = ImGui::CalcTextSize(toggleText).x;
    if (toggleTextW > modeTextW) modeTextW = toggleTextW;
    float arrowW = 18.0f;
    float modeGap = 4.0f;
    float modeTotalW = arrowW + modeGap + modeTextW + pad2 + modeGap + arrowW;

    float keyTextW = ImGui::CalcTextSize(Translations::T("Press key...")).x;
    float keyNameW = ImGui::CalcTextSize(SDK::MenuUI::MenuKeyBind::GetKeyName(kb->Key)).x;
    float keyBtnW = ((keyTextW > keyNameW) ? keyTextW : keyNameW) + pad2;

    float gap = 6.0f;
    float totalW = modeTotalW + gap + keyBtnW;
    float startX = panelX + panelW - totalW - 8.0f;
    float btnY = pos.y + 4.0f;
    float btnH = ITEM_H - 8.0f;

    ImU32 colBtn = IM_COL32(36, 41, 61, 242);
    ImU32 colKeyActive = IM_COL32(77, 148, 87, 242);
    ImU32 colKeyListening = IM_COL32(180, 130, 40, 242);

    float cx = startX;
    ImVec2 leftMin = ImVec2(cx, btnY);
    ImVec2 leftMax = ImVec2(cx + arrowW, btnY + btnH);
    dl->AddRectFilled(leftMin, leftMax, colBtn, 7.0f);
    ImVec2 lts = ImGui::CalcTextSize("<");
    dl->AddText(ImVec2(cx + (arrowW - lts.x) * 0.5f, pos.y + (ITEM_H - lts.y) * 0.5f),
                IM_COL32(245, 247, 255, 255), "<");

    cx += arrowW + modeGap;
    float modeBtnW = modeTextW + pad2;
    ImVec2 modeMin = ImVec2(cx, btnY);
    ImVec2 modeMax = ImVec2(cx + modeBtnW, btnY + btnH);
    dl->AddRectFilled(modeMin, modeMax, colBtn, 7.0f);
    ImVec2 mts = ImGui::CalcTextSize(modeText);
    dl->AddText(ImVec2(cx + (modeBtnW - mts.x) * 0.5f, pos.y + (ITEM_H - mts.y) * 0.5f),
                IM_COL32(245, 247, 255, 255), modeText);

    cx += modeBtnW + modeGap;
    ImVec2 rightMin = ImVec2(cx, btnY);
    ImVec2 rightMax = ImVec2(cx + arrowW, btnY + btnH);
    dl->AddRectFilled(rightMin, rightMax, colBtn, 7.0f);
    ImVec2 rts = ImGui::CalcTextSize(">");
    dl->AddText(ImVec2(cx + (arrowW - rts.x) * 0.5f, pos.y + (ITEM_H - rts.y) * 0.5f),
                IM_COL32(245, 247, 255, 255), ">");

    cx += arrowW + gap;
    ImVec2 keyMin = ImVec2(cx, btnY);
    ImVec2 keyMax = ImVec2(cx + keyBtnW, btnY + btnH);
    ImU32 keyCol = kb->IsListening() ? colKeyListening : (kb->Active ? colKeyActive : colBtn);
    dl->AddRectFilled(keyMin, keyMax, keyCol, 7.0f);
    ImVec2 kts = ImGui::CalcTextSize(keyText);
    dl->AddText(ImVec2(cx + (keyBtnW - kts.x) * 0.5f, pos.y + (ITEM_H - kts.y) * 0.5f),
                IM_COL32(245, 247, 255, 255), keyText);

    if (g_inputEnabled) {
        if (ImGui::IsMouseHoveringRect(leftMin, leftMax, false) && ImGui::IsMouseClicked(0)) {
            if (kb->Type == KBType::Toggle) { kb->Type = KBType::Press; kb->Active = false; }
            else { kb->Type = KBType::Toggle; }
        }
        if (ImGui::IsMouseHoveringRect(rightMin, rightMax, false) && ImGui::IsMouseClicked(0)) {
            if (kb->Type == KBType::Press) { kb->Type = KBType::Toggle; }
            else { kb->Type = KBType::Press; kb->Active = false; }
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
        s->ResetDefault();

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
        s->ResetDefault();

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

inline const void* s_expandedDropdown = nullptr;

struct DropdownState {
    int selectedIndex;
    bool expanded;
    bool changed;
};

inline void DrawDropdownIndicator(ImDrawList* dl, float rowY, float panelX, float panelW,
                                   const char* const* labels, int count, int selectedIndex) {
    if (count <= 0) return;
    int safeIdx = std::clamp(selectedIndex, 0, count - 1);
    const char* current = labels[safeIdx];

    float maxW = 0.0f;
    for (int i = 0; i < count; i++) {
        float w = ImGui::CalcTextSize(labels[i]).x;
        if (w > maxW) maxW = w;
    }
    float dropW = maxW + 12.0f;
    float dropX = panelX + panelW - dropW - 8.0f;
    ImVec2 dropMin = ImVec2(dropX, rowY + 3.0f);
    ImVec2 dropMax = ImVec2(dropX + dropW, rowY + ITEM_H - 3.0f);
    bool dropHovered = g_inputEnabled && ImGui::IsMouseHoveringRect(dropMin, dropMax, false);

    dl->AddRectFilled(dropMin, dropMax, dropHovered ? COL_ITEM_HOVER : COL_ITEM, 3.0f);
    dl->AddRect(dropMin, dropMax, COL_BORDER, 3.0f);
    ImVec2 ts = ImGui::CalcTextSize(current);
    dl->AddText(ImVec2(dropX + (dropW - ts.x) * 0.5f, rowY + (ITEM_H - ts.y) * 0.5f),
                IM_COL32(245, 247, 255, 255), current);
}

inline float DrawDropdownExpandedRows(ImDrawList* dl, float startY, float panelX, float panelW,
                                       const char* const* labels, int count, int selectedIndex,
                                       const void* owner, DropdownState& out) {
    out.selectedIndex = selectedIndex;
    out.expanded = true;
    out.changed = false;

    float y = startY;
    for (int i = 0; i < count; i++) {
        ImVec2 rMin = ImVec2(panelX, y);
        ImVec2 rMax = ImVec2(panelX + panelW, y + ITEM_H);
        bool isSel = (i == selectedIndex);
        bool rHov = g_inputEnabled && ImGui::IsMouseHoveringRect(rMin, rMax, false);

        dl->AddRectFilled(rMin, rMax, isSel ? COL_ITEM_ACTIVE : (rHov ? COL_ITEM_HOVER : COL_ITEM), 0.0f);
        dl->AddLine(ImVec2(rMin.x, rMax.y), ImVec2(rMax.x, rMax.y), COL_BORDER, 1.0f);

        ImVec2 its = ImGui::CalcTextSize(labels[i]);
        dl->AddText(ImVec2(rMin.x + 24, rMin.y + (ITEM_H - its.y) * 0.5f),
                    isSel ? COL_ACCENT : COL_TEXT, labels[i]);

        if (g_inputEnabled && rHov && ImGui::IsMouseClicked(0)) {
            out.selectedIndex = i;
            out.expanded = false;
            out.changed = (i != selectedIndex);
            s_expandedDropdown = nullptr;
        }
        y += ITEM_H;
    }
    return ITEM_H * (float)count;
}

inline float DrawListItem(ImDrawList* dl, ImVec2 pos, float panelX, float panelW,
                           SDK::MenuUI::MenuList* lst, bool drawSep = true) {
    bool expanded = (s_expandedDropdown == lst);
    int itemCount = (int)lst->Items.size();

    ImVec2 mn = ImVec2(panelX, pos.y);
    ImVec2 mx = ImVec2(panelX + panelW, pos.y + ITEM_H);
    bool hovered = g_inputEnabled && ImGui::IsMouseHoveringRect(mn, mx, false);

    if (hovered)
        dl->AddRectFilled(mn, mx, COL_ITEM_HOVER, 3.0f);

    dl->AddText(ImVec2(pos.x + 12, pos.y + 7), COL_TEXT, Translations::T(lst->DisplayName.c_str()));

    if (!lst->Items.empty()) {
        lst->Index = std::clamp(lst->Index, 0, itemCount - 1);
        static thread_local const char* s_labelPtrs[64];
        int ptrCount = itemCount < 64 ? itemCount : 64;
        for (int i = 0; i < ptrCount; i++)
            s_labelPtrs[i] = lst->Items[i].c_str();
        DrawDropdownIndicator(dl, pos.y, panelX, panelW, s_labelPtrs, ptrCount, lst->Index);
    }

    if (g_inputEnabled && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        s_expandedDropdown = expanded ? nullptr : lst;
    if (g_inputEnabled && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        int prevIdx = lst->Index;
        lst->SetIndex(0);
        if (lst->Index != prevIdx) lst->NotifyValueChanged();
    }

    if (!expanded || lst->Items.empty()) {
        if (drawSep)
            dl->AddLine(ImVec2(panelX, pos.y + ITEM_H), ImVec2(panelX + panelW, pos.y + ITEM_H), COL_BORDER, 1.0f);
        return ITEM_H;
    }

    static thread_local const char* s_labelPtrs2[64];
    int ptrCount = itemCount < 64 ? itemCount : 64;
    for (int i = 0; i < ptrCount; i++)
        s_labelPtrs2[i] = lst->Items[i].c_str();

    DropdownState ds;
    float extraH = DrawDropdownExpandedRows(dl, pos.y + ITEM_H, panelX, panelW,
                                             s_labelPtrs2, ptrCount, lst->Index, lst, ds);
    if (ds.changed) {
        lst->Index = ds.selectedIndex;
        lst->NotifyValueChanged();
    }
    if (!ds.expanded)
        expanded = false;

    float totalH = ITEM_H + extraH;
    if (drawSep)
        dl->AddLine(ImVec2(panelX, pos.y + totalH), ImVec2(panelX + panelW, pos.y + totalH), COL_BORDER, 1.0f);
    return totalH;
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
    if (auto* lst = dynamic_cast<SDK::MenuUI::MenuList*>(item))
        return (s_expandedDropdown == lst) ? ITEM_H + ITEM_H * (float)lst->Items.size() : ITEM_H;
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
        return DrawListItem(dl, pos, panelX, panelW, lst, drawSep);
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
